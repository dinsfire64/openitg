#include "global.h"
#include "LoadingWindow_DDRMini.h"

#include "RageSurface.h"
#include "RageSurface_Load_XPM.h"
#include "RageSurfaceUtils.h"
#include "RageUtil.h"
#include "RageLog.h"
#include "loading.xpm"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>

REGISTER_LOADING_WINDOW( DDRMini );

namespace
{
struct Glyph { char ch; unsigned char columns[5]; };

/* Five columns of seven bits.  Keeping the font here avoids using FontManager
 * before it has been constructed. */
const Glyph g_Glyphs[] = {
	{'A',{0x7e,0x09,0x09,0x09,0x7e}}, {'B',{0x7f,0x49,0x49,0x49,0x36}},
	{'C',{0x3e,0x41,0x41,0x41,0x22}}, {'D',{0x7f,0x41,0x41,0x22,0x1c}},
	{'E',{0x7f,0x49,0x49,0x49,0x41}}, {'F',{0x7f,0x09,0x09,0x09,0x01}},
	{'G',{0x3e,0x41,0x49,0x49,0x7a}}, {'H',{0x7f,0x08,0x08,0x08,0x7f}},
	{'I',{0x41,0x41,0x7f,0x41,0x41}}, {'J',{0x20,0x40,0x41,0x3f,0x01}},
	{'K',{0x7f,0x08,0x14,0x22,0x41}}, {'L',{0x7f,0x40,0x40,0x40,0x40}},
	{'M',{0x7f,0x02,0x0c,0x02,0x7f}}, {'N',{0x7f,0x04,0x08,0x10,0x7f}},
	{'O',{0x3e,0x41,0x41,0x41,0x3e}}, {'P',{0x7f,0x09,0x09,0x09,0x06}},
	{'Q',{0x3e,0x41,0x51,0x21,0x5e}}, {'R',{0x7f,0x09,0x19,0x29,0x46}},
	{'S',{0x46,0x49,0x49,0x49,0x31}}, {'T',{0x01,0x01,0x7f,0x01,0x01}},
	{'U',{0x3f,0x40,0x40,0x40,0x3f}}, {'V',{0x1f,0x20,0x40,0x20,0x1f}},
	{'W',{0x7f,0x20,0x18,0x20,0x7f}}, {'X',{0x63,0x14,0x08,0x14,0x63}},
	{'Y',{0x03,0x04,0x78,0x04,0x03}}, {'Z',{0x61,0x51,0x49,0x45,0x43}},
	{'0',{0x3e,0x51,0x49,0x45,0x3e}}, {'1',{0x00,0x42,0x7f,0x40,0x00}},
	{'2',{0x62,0x51,0x49,0x49,0x46}}, {'3',{0x22,0x41,0x49,0x49,0x36}},
	{'4',{0x18,0x14,0x12,0x7f,0x10}}, {'5',{0x2f,0x49,0x49,0x49,0x31}},
	{'6',{0x3e,0x49,0x49,0x49,0x32}}, {'7',{0x01,0x01,0x71,0x09,0x07}},
	{'8',{0x36,0x49,0x49,0x49,0x36}}, {'9',{0x26,0x49,0x49,0x49,0x3e}},
	{'-',{0x08,0x08,0x08,0x08,0x08}}, {'.',{0x00,0x60,0x60,0x00,0x00}},
	{',',{0x00,0x40,0x20,0x00,0x00}}, {':',{0x00,0x36,0x36,0x00,0x00}},
	{'/',{0x20,0x10,0x08,0x04,0x02}}, {'(',{0x00,0x1c,0x22,0x41,0x00}},
	{')',{0x00,0x41,0x22,0x1c,0x00}}, {'%',{0x23,0x13,0x08,0x64,0x62}},
	{'?',{0x02,0x01,0x51,0x09,0x06}}
};

const unsigned char *FindGlyph( char ch )
{
	for( unsigned i = 0; i < sizeof(g_Glyphs)/sizeof(g_Glyphs[0]); ++i )
		if( g_Glyphs[i].ch == ch ) return g_Glyphs[i].columns;
	return g_Glyphs[sizeof(g_Glyphs)/sizeof(g_Glyphs[0])-1].columns;
}
}

LoadingWindow_DDRMini::LoadingWindow_DDRMini():
	m_pWindow(NULL), m_Context(NULL), m_iIconTexture(0), m_iIconWidth(0),
	m_iIconHeight(0), m_iTextureWidth(0), m_iTextureHeight(0),
	m_iDrawableWidth(0), m_iDrawableHeight(0), m_iCurrent(0), m_iTarget(0),
	m_iAnimationStep(0), m_bInitializedVideo(false)
{
}

LoadingWindow_DDRMini::~LoadingWindow_DDRMini()
{
	Destroy();
}

CString LoadingWindow_DDRMini::Init()
{
	SDL_SetMainReady();
	if( (SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0 )
	{
		if( SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0 )
			return SDL_GetError();
		m_bInitializedVideo = true;
	}

	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 1 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 1 );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	m_pWindow = SDL_CreateWindow( "OpenITG", SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED, 1280, 720,
		SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP );
	if( m_pWindow == NULL )
	{
		CString error = SDL_GetError(); Destroy(); return error;
	}
	m_Context = SDL_GL_CreateContext( m_pWindow );
	if( m_Context == NULL )
	{
		CString error = SDL_GetError(); Destroy(); return error;
	}
	SDL_GL_SetSwapInterval( 1 );
	SDL_GL_GetDrawableSize( m_pWindow, &m_iDrawableWidth, &m_iDrawableHeight );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_CULL_FACE );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glEnableClientState( GL_VERTEX_ARRAY );

	CString bannerError;
	RageSurface *banner = RageSurface_Load_XPM(
		const_cast<char * const *>(loading), bannerError );
	if( banner == NULL )
		LOG->Warn( "Couldn't load DDR Mini loading banner: %s",
			bannerError.c_str() );
	else
	{
		SetBanner( banner );
		delete banner;
	}
	Paint();
	return CString();
}

void LoadingWindow_DDRMini::Destroy()
{
	if( m_Context != NULL )
	{
		SDL_GL_MakeCurrent( m_pWindow, m_Context );
		if( m_iIconTexture != 0 ) glDeleteTextures( 1, &m_iIconTexture );
		SDL_GL_DeleteContext( m_Context );
		m_Context = NULL;
	}
	if( m_pWindow != NULL ) { SDL_DestroyWindow( m_pWindow ); m_pWindow = NULL; }
	if( m_bInitializedVideo )
	{
		SDL_QuitSubSystem( SDL_INIT_VIDEO );
		m_bInitializedVideo = false;
	}
}

void LoadingWindow_DDRMini::PumpEvents()
{
	SDL_Event event;
	while( SDL_PollEvent(&event) ) { }
}

void LoadingWindow_DDRMini::DrawQuad( float left, float top, float right,
	float bottom, float red, float green, float blue, float alpha, bool textured )
{
	const GLfloat vertices[] = { left,top, right,top, left,bottom, right,bottom };
	const float texRight = textured ? float(m_iIconWidth) / m_iTextureWidth : 0;
	const float texBottom = textured ? float(m_iIconHeight) / m_iTextureHeight : 0;
	const GLfloat texcoords[] = { 0,0, texRight,0, 0,texBottom, texRight,texBottom };
	glColor4f( red, green, blue, alpha );
	glVertexPointer( 2, GL_FLOAT, 0, vertices );
	if( textured )
	{
		glEnable( GL_TEXTURE_2D ); glEnableClientState( GL_TEXTURE_COORD_ARRAY );
		glTexCoordPointer( 2, GL_FLOAT, 0, texcoords );
	}
	else
	{
		glDisable( GL_TEXTURE_2D ); glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	}
	glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
}

void LoadingWindow_DDRMini::DrawText( float centerX, float centerY, float maximumWidth )
{
	if( m_sText.empty() ) return;
	vector<CString> lines;
	split( m_sText, "\n", lines, false );
	if( lines.empty() ) lines.push_back( m_sText );
	if( lines.size() > 3 ) lines.resize( 3 );
	size_t longest = 0;
	for( unsigned i = 0; i < lines.size(); ++i )
	{
		if( lines[i].size() > 72 ) lines[i] = lines[i].Left(69) + "...";
		longest = max( longest, lines[i].size() );
	}
	if( longest == 0 ) return;
	const float cell = max( 2.0f, min(4.0f, maximumWidth / (longest * 6.0f)) );
	const float blockHeight = 7*cell + (lines.size()-1)*9*cell;
	for( unsigned line = 0; line < lines.size(); ++line )
	{
		const float width = lines[line].empty()? 0 : lines[line].size()*6*cell-cell;
		float x = centerX-width/2;
		const float y = centerY-blockHeight/2 + line*9*cell;
		for( unsigned i = 0; i < lines[line].size(); ++i, x += 6*cell )
		{
			unsigned char raw = lines[line][i];
			if( raw == ' ' || raw == '\r' || raw == '\t' ) continue;
			char ch = raw < 128 ? char(toupper(raw)) : '?';
			const unsigned char *columns = FindGlyph( ch );
			for( int column = 0; column < 5; ++column )
				for( int row = 0; row < 7; ++row )
					if( columns[column] & (1 << row) )
						DrawQuad( x+column*cell, y+row*cell,
							x+(column+1)*cell, y+(row+1)*cell, 1,1,1,1 );
		}
	}
}

void LoadingWindow_DDRMini::Paint()
{
	if( m_pWindow == NULL || m_Context == NULL ) return;
	PumpEvents();
	SDL_GL_MakeCurrent( m_pWindow, m_Context );
	SDL_GL_GetDrawableSize( m_pWindow, &m_iDrawableWidth, &m_iDrawableHeight );
	glViewport( 0, 0, m_iDrawableWidth, m_iDrawableHeight );
	glMatrixMode( GL_PROJECTION ); glLoadIdentity();
	glOrthof( 0, m_iDrawableWidth, m_iDrawableHeight, 0, -1, 1 );
	glMatrixMode( GL_MODELVIEW ); glLoadIdentity();
	/* Match the cabinet X root background set by:
	 * xsetroot -solid '#005050' */
	glClearColor( 0.0f, 80.0f/255.0f, 80.0f/255.0f, 1.0f );
	glClear( GL_COLOR_BUFFER_BIT );

	const float left = m_iDrawableWidth*.1f, right = m_iDrawableWidth*.9f;
	const float top = m_iDrawableHeight*.88f, bottom = m_iDrawableHeight*.92f;
	float iconBottom = m_iDrawableHeight*.08f;
	if( m_iIconTexture != 0 )
	{
		glBindTexture( GL_TEXTURE_2D, m_iIconTexture );
		const float scale = min( 1.0f, min((right-left)/m_iIconWidth,
			(m_iDrawableHeight*.52f)/m_iIconHeight) );
		const float width=m_iIconWidth*scale, height=m_iIconHeight*scale;
		const float iconLeft=(m_iDrawableWidth-width)/2, iconTop=m_iDrawableHeight*.06f;
		iconBottom=iconTop+height;
		DrawQuad( iconLeft, iconTop, iconLeft+width, iconBottom, 1,1,1,1, true );
	}
	DrawText( m_iDrawableWidth*.5f, iconBottom+(top-iconBottom)*.5f, right-left );
	DrawQuad( left-4, top-4, right+4, bottom+4, 0,0,0,.75f );
	float fillRight;
	if( m_iTarget > 0 )
		fillRight = left + min(1.0f, float(m_iCurrent)/m_iTarget)*(right-left);
	else
	{
		const float segment=(right-left)*.2f;
		fillRight = left + float(m_iAnimationStep++%40)/39*(right-left-segment) + segment;
		DrawQuad( fillRight-segment, top, fillRight, bottom, .1f,.75f,1,1 );
		SDL_GL_SwapWindow( m_pWindow ); return;
	}
	DrawQuad( left, top, fillRight, bottom, .1f,.75f,1,1 );
	SDL_GL_SwapWindow( m_pWindow );
}

void LoadingWindow_DDRMini::SetText( CString str )
{
	m_sText = str;
	Paint();
}

void LoadingWindow_DDRMini::SetProgress( unsigned current, unsigned target )
{
	m_iCurrent = current; m_iTarget = target;
}

void LoadingWindow_DDRMini::SetIcon( const RageSurface * )
{
	/* The DDR Mini window is fullscreen, so it has no visible window-manager
	 * icon.  Keep the startup banner in place when the theme icon is supplied. */
}

void LoadingWindow_DDRMini::SetBanner( const RageSurface *icon )
{
	if( icon == NULL || m_Context == NULL ) return;
	RageSurface *converted = NULL;
	RageSurfaceUtils::ConvertSurface( const_cast<RageSurface *>(icon), converted,
		icon->w, icon->h, 32,
		0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 );
	const RageSurface *upload = converted != NULL ? converted : icon;
	m_iIconWidth=upload->w; m_iIconHeight=upload->h;
	m_iTextureWidth=power_of_two(m_iIconWidth); m_iTextureHeight=power_of_two(m_iIconHeight);
	vector<unsigned char> pixels( m_iIconWidth*m_iIconHeight*4 );
	for( int row=0; row<m_iIconHeight; ++row )
		memcpy( &pixels[row*m_iIconWidth*4],
			(const unsigned char*)upload->pixels + row*upload->pitch, m_iIconWidth*4 );
	SDL_GL_MakeCurrent( m_pWindow, m_Context );
	if( m_iIconTexture != 0 ) glDeleteTextures( 1, &m_iIconTexture );
	glGenTextures( 1, &m_iIconTexture ); glBindTexture( GL_TEXTURE_2D, m_iIconTexture );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, m_iTextureWidth, m_iTextureHeight,
		0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
	glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, m_iIconWidth, m_iIconHeight,
		GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0] );
	delete converted;
	Paint();
}
