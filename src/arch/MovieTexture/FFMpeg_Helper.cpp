/* vim: set noet ts=4 sw=4: */
#include "global.h"
#include "RageLog.h"
#include "FFMpeg_Helper.h"
#include "RageFile.h"
#include "RageSurface.h"

#include <cerrno>
extern "C"
{
#include <libswscale/swscale.h>
}


AVPixelFormat_t AVPixelFormats[5] = {
	{ 
		32,
		{
			0x00FF0000,
			0x0000FF00,
			0x000000FF,
			0xFF000000 },
		avcodec::AV_PIX_FMT_ARGB,
		true,
		true
	},
	{ 
		24,
		{
			0xFF0000,
			0x00FF00,
			0x0000FF,
			0x000000 },
		avcodec::AV_PIX_FMT_RGB24,
		true,
		true
	},
	{ 
		24,
		{
			0x0000FF,
			0x00FF00,
			0xFF0000,
			0x000000 },
		avcodec::AV_PIX_FMT_BGR24,
		true,
		true
	},
	{
		16,
		{
			0x7C00,
			0x03E0,
			0x001F,
			0x0000 },
		avcodec::AV_PIX_FMT_RGB555,
		false,
		false
	},
	{ 0, { 0,0,0,0 }, avcodec::AV_PIX_FMT_NB, true, false }
};

static CString averr_ssprintf( int err, const char *fmt, ... )
{
	ASSERT( err < 0 );

	va_list va;
	va_start(va, fmt);
	CString s = vssprintf( fmt, va );
	va_end(va);
	char szError[1024];

	avcodec::av_strerror(err, szError, 1024);
	return s + " (" + szError + ")";
}

int URLRageFile_read( void *opaque, uint8_t *buf, int size )
{
	RageFile *f = (RageFile *) opaque;
	return f->Read( buf, size );
}

int64_t URLRageFile_seek( void *opaque, int64_t pos, int whence )
{
	RageFile *f = (RageFile *) opaque;
	if (whence == AVSEEK_SIZE)
	{
		return f->GetFileSize();
	}
	return f->Seek( (int) pos, whence );
}

FFMpeg_Helper::FFMpeg_Helper()
{
	m_fctx=NULL;
	m_cctx=NULL;
	m_ioctx=NULL;
	m_stream=NULL;
	m_swsctx=NULL;
	this->frame=NULL;
	current_packet_offset = -1;
	Init();
}

FFMpeg_Helper::~FFMpeg_Helper()
{
	if( current_packet_offset != -1 )
	{
		avcodec::av_packet_unref( &pkt );
		current_packet_offset = -1;
	}
	if( frame != NULL )
		avcodec::av_frame_free( &frame );
	if( m_swsctx != NULL )
		avcodec::sws_freeContext( m_swsctx );
}

void FFMpeg_Helper::Init()
{
	eof = 0;
	m_width = -1;
	m_height = -1;
	GetNextTimestamp = true;
	CurrentTimestamp = 0, Last_IP_Timestamp = 0;
	LastFrameDelay = 0;
	pts = -1;
	FrameNumber = -1; /* decode one frame and you're on the 0th */
	TimestampOffset = 0;

	if( current_packet_offset != -1 )
	{
		avcodec::av_packet_unref( &pkt );
		current_packet_offset = -1;
	}
}

CString FFMpeg_Helper::Open(CString what)
{
	uint8_t *ctxbuffer = (uint8_t *)avcodec::av_malloc(FFCTX_BUFFER_SIZE);
	if (ctxbuffer == NULL)
	{
		return ssprintf( "AVCodec(%s): Could not allocate context buffer", what.c_str() );
	}

	m_pFile = new RageFile;
	if( !m_pFile->Open(what, RageFile::READ) )
	{
		CString sErr = m_pFile->GetError();
		SAFE_DELETE(m_pFile);
		return ssprintf("Error opening \"%s\": %s", what.c_str(), sErr.c_str());
	}
	CHECKPOINT_M( what );

	m_fctx = avcodec::avformat_alloc_context();
	if (m_fctx == NULL)
	{
		return ssprintf( "AVCodec(%s): Could not allocate avformat context", what.c_str() );
	}
	CHECKPOINT_M( ssprintf("%d", m_fctx->bit_rate) );

	m_ioctx = avcodec::avio_alloc_context( ctxbuffer, FFCTX_BUFFER_SIZE, 0,
			m_pFile, &URLRageFile_read, NULL, &URLRageFile_seek );
	if ( m_ioctx == NULL )
	{
		return ssprintf( "AVCodec (%s): Couldn't allocate AVIO context", what.c_str() );
	}

	CHECKPOINT_M( ssprintf("%lu", m_ioctx->pos) );

	m_fctx->pb = m_ioctx;
	int ret = avcodec::avformat_open_input( &m_fctx, NULL, NULL, NULL );
	if ( ret < 0 )
	{
		return ssprintf( averr_ssprintf(ret, "AVCodec (%s): Couldn't open input", what.c_str()) );
	}

	ret = avcodec::avformat_find_stream_info( m_fctx, NULL );
	if ( ret < 0 )
	{
		return ssprintf( averr_ssprintf(ret, "AVCodec (%s): Couldn't find codec parameters", what.c_str()) );
	}

	avcodec::AVCodec *codec;
	ret = avcodec::av_find_best_stream( m_fctx, avcodec::AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0 );
	if ( ret < 0 )
	{
		return ssprintf( averr_ssprintf(ret, "AVCodec (%s): Couldn't find codec", what.c_str()) );
	}

	CHECKPOINT_M( ssprintf("%d", ret) );

	avcodec::AVStream *stream = m_fctx->streams[ret];
	if ( stream == NULL )
	{
		return ssprintf( "AVCodec (%s): Couldn't find any video streams", what.c_str() );
	}

	m_cctx = avcodec::avcodec_alloc_context3( codec );
	if ( m_cctx == NULL )
	{
		return ssprintf( "AVCodec (%s): Couldn't allocate codec context", what.c_str() );
	}

	ret = avcodec::avcodec_parameters_to_context( m_cctx, stream->codecpar );
	if ( ret < 0 )
	{
		return ssprintf( averr_ssprintf(ret, "AVCodec (%s): Couldn't copy codec parameters", what.c_str()) );
	}

	m_width  = m_cctx->width;
	m_height = m_cctx->height;

	LOG->Trace("Opening codec %s (%dx%d)", codec->name, m_width, m_height );

	ret = avcodec::avcodec_open2( m_cctx, codec, NULL );
	if ( ret < 0 )
	{
		return ssprintf( averr_ssprintf(ret, "AVCodec (%s): Couldn't open codec \"%s\"", what.c_str(), codec->name) );
	}

	/* Don't set this until we successfully open stream->codec, so we don't try to close it
	 * on an exception unless it was really opened. */
	m_stream = stream;

	LOG->Trace("Bitrate: %ld", m_cctx->bit_rate );
	LOG->Trace("Codec pixel format: %s", avcodec::av_get_pix_fmt_name(m_cctx->pix_fmt) );

	if (this->frame == NULL)
	{
		CHECKPOINT;
		this->frame = avcodec::av_frame_alloc();
		if (this->frame == NULL)
		{
			return ssprintf( "AVCodec (%s): Couldn't allocate frame", what.c_str() );
		}
	}
	return "";
}

void FFMpeg_Helper::Close()
{
	if( m_cctx )
	{
		avcodec::avcodec_free_context( &m_cctx );
	}
	if( m_fctx )
	{
		avcodec::avformat_close_input( &m_fctx );
		m_fctx = NULL;
	}
	if( m_stream )
	{
		m_stream = NULL;
	}
	if ( m_ioctx )
	{
		avcodec::avio_context_free( &m_ioctx );
	}
	if ( m_pFile )
	{
		CHECKPOINT_M( m_pFile->GetPath() );
		SAFE_DELETE( m_pFile );
	}
}

void FFMpeg_Helper::RegisterProtocols()
{
	static bool Done = false;
	if( Done )
		return;
	Done = true;

	/* Current FFmpeg registers its formats and codecs automatically. */
}

int FFMpeg_Helper::GetRawFrameNumber()
{
	return m_cctx->frame_number;
}

/* Read until we get a frame, EOF or error.  Return -1 on error, 0 on EOF, 1 if we have a frame. */
int FFMpeg_Helper::GetFrame()
{
	while( 1 )
	{
		int ret = DecodePacket();
		if( ret == 1 )
			break;
		if( ret == -1 )
			return -1;
		if( ret == 2 )
			continue;
		if( ret == 0 && eof > 0 )
			return 0; /* eof */

		ASSERT( ret == 0 );
		ret = ReadPacket();
		if( ret < 0 )
			return ret; /* error */
	}

	++FrameNumber;

	if( FrameNumber == 1 )
	{
		/* Some videos start with a timestamp other than 0.  I think this is used
		 * when audio starts before the video.  We don't want to honor that, since
		 * the DShow renderer doesn't and we don't want to break sync compatibility.
		 *
		 * Look at the second frame.  (If we have B-frames, the first frame will be an
		 * I-frame with the timestamp of the next P-frame, not its own timestamp, and we
		 * want to ignore that and look at the next B-frame.) */
		const float expect = LastFrameDelay;
		const float actual = CurrentTimestamp;
		if( actual - expect > 0 )
		{
			LOG->Trace("Expect %f, got %f -> %f", expect, actual, actual - expect );
			TimestampOffset = actual - expect;
		}
	}

	return 1;
}

float FFMpeg_Helper::GetTimestamp() const
{
	/* The first frame always has a timestamp of 0. */
	if( FrameNumber == 0 )
		return 0;

	return CurrentTimestamp - TimestampOffset;
}

/* Read a packet.  Return -1 on error, 0 on EOF, 1 on OK. */
int FFMpeg_Helper::ReadPacket()
{
	if( eof > 0 )
		return 0;

	while( 1 )
	{
		if( current_packet_offset != -1 )
		{
			current_packet_offset = -1;
			avcodec::av_packet_unref( &pkt );
		}

		int ret = avcodec::av_read_frame( m_fctx, &pkt );
		/* XXX: why is avformat returning AVERROR_NOMEM on EOF? */
		if( ret < 0 )
		{
			/* EOF. */
			eof = 1;
			pkt.size = 0;

			return 0;
		}

		if( pkt.stream_index == m_stream->index )
		{
			current_packet_offset = 0;
			return 1;
		}

		/* It's not for the video stream; ignore it. */
		avcodec::av_packet_unref( &pkt );
	}
}

void FFMpeg_Helper::RenderFrame(RageSurface *img, int tex_fmt)
{
	uint8_t *out_b[4] = { img->pixels, NULL, NULL, NULL };
	int out_s[4] = { img->pitch, 0, 0, 0 };

	if (m_swsctx == NULL)
	{
		m_swsctx = avcodec::sws_getCachedContext(m_swsctx,
				m_width, m_height, m_cctx->pix_fmt,
				m_width, m_height, AVPixelFormats[tex_fmt].pf,
				SWS_BICUBIC, NULL, NULL, NULL);
	}

	if (m_swsctx == NULL)
	{
		LOG->Warn("FFMpeg_Helper: Could not allocate scaling context");
	}
	else
	{
		avcodec::sws_scale(m_swsctx,
				this->frame->data, this->frame->linesize,
				0, m_height,
				out_b, out_s);
	}
}

/* Decode data from the current packet.  Return -1 on error, 0 if the packet is finished,
 * and 1 if we have a frame (we may have more data in the packet). */
int FFMpeg_Helper::DecodePacket()
{
	int ret = avcodec::avcodec_receive_frame( m_cctx, frame );
	if( ret == 0 )
	{
		int64_t timestamp = frame->best_effort_timestamp;
		if ( timestamp != int64_t(AV_NOPTS_VALUE) )
		{
			CurrentTimestamp = (float)timestamp * m_stream->time_base.num /
				m_stream->time_base.den;
		}
		else
		{
			/* If the timestamp is zero, this frame is to be played at the
			 * time of the last frame plus the length of the last frame. */
			CurrentTimestamp += LastFrameDelay;
		}

		/* Length of this frame: */
		avcodec::AVRational rate = avcodec::av_guess_frame_rate( m_fctx, m_stream, frame );
		if( rate.num > 0 && rate.den > 0 )
			LastFrameDelay = (float)rate.den / rate.num;
		else if( m_cctx->time_base.num > 0 && m_cctx->time_base.den > 0 )
			LastFrameDelay = (float)m_cctx->time_base.num / m_cctx->time_base.den;
		else
			LastFrameDelay = 1.0f / 30.0f;
		LastFrameDelay += frame->repeat_pict * (LastFrameDelay * 0.5f);

		return 1;
	}

	if( ret != AVERROR(EAGAIN) && ret != AVERROR_EOF )
	{
		LOG->Warn("avcodec_receive_frame: %i", ret);
		return -1;
	}

	if( current_packet_offset != -1 )
	{
		ret = avcodec::avcodec_send_packet( m_cctx, &pkt );
		if( ret != AVERROR(EAGAIN) )
		{
			avcodec::av_packet_unref( &pkt );
			current_packet_offset = -1;
		}
		if( ret < 0 && ret != AVERROR(EAGAIN) )
		{
			LOG->Warn("avcodec_send_packet: %i", ret);
			return -1;
		}
		return 2; /* packet accepted; try receiving again before reading */
	}

	if( eof == 1 )
	{
		ret = avcodec::avcodec_send_packet( m_cctx, NULL );
		eof = 2;
		if( ret < 0 && ret != AVERROR_EOF )
		{
			LOG->Warn("avcodec_send_packet (flush): %i", ret);
			return -1;
		}
		return 2;
	}

	return 0;
}
