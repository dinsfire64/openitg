#include "global.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <GLES/gl.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <vector>

#include "RageDisplay_DDRMini.h"
#include "RageLog.h"
#include "RageMath.h"
#include "RageSurface.h"
#include "RageSurfaceUtils.h"
#include "RageTexture.h"
#include "StepMania.h"

namespace
{
RageDisplay::PixelFormatDesc g_PixelFormats[RageDisplay::NUM_PIX_FORMATS] = {
	{ 32, { 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000 } },
	{ 16, { 0xF000, 0x0F00, 0x00F0, 0x000F } },
	{ 16, { 0xF800, 0x07C0, 0x003E, 0x0001 } },
	{ 16, { 0xF800, 0x07E0, 0x001F, 0x0000 } },
	{ 24, { 0x0000FF, 0x00FF00, 0xFF0000, 0x000000 } },
	{ 8,  { 0, 0, 0, 0 } },
	{ 24, { 0xFF0000, 0x00FF00, 0x0000FF, 0x000000 } },
	{ 16, { 0x001F, 0x03E0, 0x7C00, 0x8000 } }
};

struct GLFormat
{
	GLenum format;
	GLenum type;
	int bytes;
};

bool GetGLFormat( RageDisplay::RagePixelFormat pf, GLFormat &out )
{
	switch( pf )
	{
	case RageDisplay::FMT_RGBA8: out.format=GL_RGBA; out.type=GL_UNSIGNED_BYTE; out.bytes=4; return true;
	case RageDisplay::FMT_RGBA4: out.format=GL_RGBA; out.type=GL_UNSIGNED_SHORT_4_4_4_4; out.bytes=2; return true;
	case RageDisplay::FMT_RGB5A1: out.format=GL_RGBA; out.type=GL_UNSIGNED_SHORT_5_5_5_1; out.bytes=2; return true;
	case RageDisplay::FMT_RGB5: out.format=GL_RGB; out.type=GL_UNSIGNED_SHORT_5_6_5; out.bytes=2; return true;
	case RageDisplay::FMT_RGB8: out.format=GL_RGB; out.type=GL_UNSIGNED_BYTE; out.bytes=3; return true;
	default: return false;
	}
}

map<GLuint, RageDisplay::RagePixelFormat> g_TextureFormats;
bool g_SphereEnvironmentMapping = false;

void GenerateSphereMapTexcoords( const RageVector3 *positions,
	const RageVector3 *normals, int count, vector<RageVector2> &texcoords )
{
	GLfloat modelView[16];
	glGetFloatv( GL_MODELVIEW_MATRIX, modelView );
	texcoords.resize( count );

	for( int i = 0; i < count; ++i )
	{
		/* Transform into eye space.  Models which use sphere maps in OpenITG
		 * use uniform scaling, so the model-view 3x3 is also sufficient for
		 * transforming and then normalizing their normals. */
		RageVector3 p(
			modelView[0]*positions[i].x + modelView[4]*positions[i].y + modelView[8]*positions[i].z + modelView[12],
			modelView[1]*positions[i].x + modelView[5]*positions[i].y + modelView[9]*positions[i].z + modelView[13],
			modelView[2]*positions[i].x + modelView[6]*positions[i].y + modelView[10]*positions[i].z + modelView[14] );
		RageVector3 n(
			modelView[0]*normals[i].x + modelView[4]*normals[i].y + modelView[8]*normals[i].z,
			modelView[1]*normals[i].x + modelView[5]*normals[i].y + modelView[9]*normals[i].z,
			modelView[2]*normals[i].x + modelView[6]*normals[i].y + modelView[10]*normals[i].z );

		const float nLength = sqrtf( n.x*n.x + n.y*n.y + n.z*n.z );
		const float pLength = sqrtf( p.x*p.x + p.y*p.y + p.z*p.z );
		if( nLength <= 0.000001f || pLength <= 0.000001f )
		{
			texcoords[i] = RageVector2( 0.5f, 0.5f );
			continue;
		}
		n.x /= nLength; n.y /= nLength; n.z /= nLength;
		p.x /= pLength; p.y /= pLength; p.z /= pLength;

		/* Match desktop OpenGL's GL_SPHERE_MAP generation. */
		const float dot = p.x*n.x + p.y*n.y + p.z*n.z;
		const RageVector3 r( p.x - 2*dot*n.x, p.y - 2*dot*n.y, p.z - 2*dot*n.z );
		const float m = 2 * sqrtf( r.x*r.x + r.y*r.y + (r.z+1)*(r.z+1) );
		if( m <= 0.000001f )
			texcoords[i] = RageVector2( 0.5f, 0.5f );
		else
			texcoords[i] = RageVector2( r.x/m + 0.5f, r.y/m + 0.5f );
	}
}

RageSurface *ConvertUploadSurface( RageSurface *source, RageDisplay::RagePixelFormat pf )
{
	const RageDisplay::PixelFormatDesc &desc = g_PixelFormats[pf];
	RageSurface *converted = NULL;
	if( !RageSurfaceUtils::ConvertSurface(source, converted, source->w, source->h,
		desc.bpp, desc.masks[0], desc.masks[1], desc.masks[2], desc.masks[3]) )
		return NULL;
	return converted;
}

void SetupVertices( const RageSpriteVertex *vertices, int count )
{
	if( vertices == NULL || count <= 0 )
		return;

	static vector<float> positions;
	static vector<float> normals;
	static vector<float> texcoords;
	static vector<unsigned char> colors;
	positions.resize(count*3);
	normals.resize(count*3);
	texcoords.resize(count*2);
	colors.resize(count*4);

	for( int i=0; i<count; ++i )
	{
		positions[i*3+0] = vertices[i].p.x;
		positions[i*3+1] = vertices[i].p.y;
		positions[i*3+2] = vertices[i].p.z;
		normals[i*3+0] = vertices[i].n.x;
		normals[i*3+1] = vertices[i].n.y;
		normals[i*3+2] = vertices[i].n.z;
		texcoords[i*2+0] = vertices[i].t.x;
		texcoords[i*2+1] = vertices[i].t.y;
		colors[i*4+0] = vertices[i].c.r;
		colors[i*4+1] = vertices[i].c.g;
		colors[i*4+2] = vertices[i].c.b;
		colors[i*4+3] = vertices[i].c.a;
	}

	glVertexPointer( 3, GL_FLOAT, 0, &positions[0] );
	glNormalPointer( GL_FLOAT, 0, &normals[0] );
	glTexCoordPointer( 2, GL_FLOAT, 0, &texcoords[0] );
	glColorPointer( 4, GL_UNSIGNED_BYTE, 0, &colors[0] );
}

class RageCompiledGeometryGLES: public RageCompiledGeometry
{
public:
	void Allocate( const vector<msMesh> &meshes )
	{
		m_Positions.resize( GetTotalVertices() );
		m_Normals.resize( GetTotalVertices() );
		m_Texcoords.resize( GetTotalVertices() );
		m_TexMatrixScales.resize( GetTotalVertices() );
		m_Triangles.resize( GetTotalTriangles() );
	}

	void Change( const vector<msMesh> &meshes )
	{
		for( unsigned i=0; i<meshes.size(); ++i )
		{
			const MeshInfo &info = m_vMeshInfo[i];
			for( unsigned j=0; j<meshes[i].Vertices.size(); ++j )
			{
				m_Positions[info.iVertexStart+j] = meshes[i].Vertices[j].p;
				m_Normals[info.iVertexStart+j] = meshes[i].Vertices[j].n;
				m_Texcoords[info.iVertexStart+j] = meshes[i].Vertices[j].t;
				m_TexMatrixScales[info.iVertexStart+j] = meshes[i].Vertices[j].TextureMatrixScale;
			}
			for( unsigned j=0; j<meshes[i].Triangles.size(); ++j )
				for( int k=0; k<3; ++k )
					m_Triangles[info.iTriangleStart+j].nVertexIndices[k] =
						(uint16_t)(info.iVertexStart + meshes[i].Triangles[j].nVertexIndices[k]);
		}
	}

	void Draw( int mesh ) const
	{
		if( m_Positions.empty() ) return;
		const MeshInfo &info = m_vMeshInfo[mesh];
		glVertexPointer( 3, GL_FLOAT, 0, &m_Positions[0] );
		glNormalPointer( GL_FLOAT, 0, &m_Normals[0] );
		bool restoreTextureMatrix = false;
		if( g_SphereEnvironmentMapping )
		{
			GenerateSphereMapTexcoords( &m_Positions[0], &m_Normals[0],
				m_Positions.size(), m_SphereTexcoords );
			glTexCoordPointer( 2, GL_FLOAT, 0, &m_SphereTexcoords[0] );
		}
		else if( info.bNeedsTextureMatrixScale )
		{
			/* GLES 1.1 has no vertex attributes or shaders.  Apply OpenITG's
			 * per-vertex texture-matrix scale on the CPU, then draw with an
			 * identity texture matrix so the translation is not applied twice. */
			GLfloat matrix[16];
			glGetFloatv( GL_TEXTURE_MATRIX, matrix );
			m_TransformedTexcoords.resize( m_Texcoords.size() );
			for( unsigned i = 0; i < m_Texcoords.size(); ++i )
			{
				const float u = m_Texcoords[i].x;
				const float v = m_Texcoords[i].y;
				const float transformedU = matrix[0]*u + matrix[4]*v + matrix[12];
				const float transformedV = matrix[1]*u + matrix[5]*v + matrix[13];
				const float scaleU = m_TexMatrixScales[i].x;
				const float scaleV = m_TexMatrixScales[i].y;
				m_TransformedTexcoords[i].x = transformedU*scaleU + u*(1-scaleU);
				m_TransformedTexcoords[i].y = transformedV*scaleV + v*(1-scaleV);
			}
			glTexCoordPointer( 2, GL_FLOAT, 0, &m_TransformedTexcoords[0] );
			glMatrixMode( GL_TEXTURE );
			glPushMatrix();
			glLoadIdentity();
			glMatrixMode( GL_MODELVIEW );
			restoreTextureMatrix = true;
		}
		else
			glTexCoordPointer( 2, GL_FLOAT, 0, &m_Texcoords[0] );
		glDisableClientState( GL_COLOR_ARRAY );
		glDrawElements( GL_TRIANGLES, info.iTriangleCount*3, GL_UNSIGNED_SHORT,
			&m_Triangles[info.iTriangleStart] );
		if( restoreTextureMatrix )
		{
			glMatrixMode( GL_TEXTURE );
			glPopMatrix();
			glMatrixMode( GL_MODELVIEW );
		}
		glEnableClientState( GL_COLOR_ARRAY );
	}

private:
	vector<RageVector3> m_Positions;
	vector<RageVector3> m_Normals;
	vector<RageVector2> m_Texcoords;
	vector<RageVector2> m_TexMatrixScales;
	mutable vector<RageVector2> m_SphereTexcoords;
	mutable vector<RageVector2> m_TransformedTexcoords;
	vector<msTriangle> m_Triangles;
};
}

RageDisplay_DDRMini::RageDisplay_DDRMini(): m_Window(NULL), m_Context(NULL)
{
	LOG->MapLog( "renderer", "Current renderer: DDR Mini GLES" );
}

RageDisplay_DDRMini::~RageDisplay_DDRMini()
{
	g_TextureFormats.clear();
	if( m_Context ) SDL_GL_DeleteContext( m_Context );
	if( m_Window ) SDL_DestroyWindow( m_Window );
	SDL_QuitSubSystem( SDL_INIT_VIDEO );
}

CString RageDisplay_DDRMini::Init( VideoModeParams params )
{
	SDL_SetMainReady();
	if( SDL_InitSubSystem(SDL_INIT_VIDEO|SDL_INIT_EVENTS) != 0 )
		return SDL_GetError();
	bool reload = false;
	return SetVideoMode( params, reload );
}

CString RageDisplay_DDRMini::TryVideoMode( VideoModeParams params, bool &newDevice )
{
	if( m_Context ) { g_TextureFormats.clear(); SDL_GL_DeleteContext(m_Context); m_Context=NULL; }
	if( m_Window ) { SDL_DestroyWindow(m_Window); m_Window=NULL; }

	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 1 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 1 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

	m_Window = SDL_CreateWindow( params.sWindowTitle.c_str(), SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED, params.width, params.height,
		SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN_DESKTOP );
	if( !m_Window ) return SDL_GetError();
	m_Context = SDL_GL_CreateContext( m_Window );
	if( !m_Context ) return SDL_GetError();

	SDL_GL_SetSwapInterval( params.vsync? 1:0 );
	SDL_GL_GetDrawableSize( m_Window, &params.width, &params.height );
	params.windowed = false;
	params.bpp = 32;
	m_Params = params;
	newDevice = true;

	glViewport( 0, 0, params.width, params.height );
	glEnableClientState( GL_VERTEX_ARRAY );
	glEnableClientState( GL_COLOR_ARRAY );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glEnableClientState( GL_NORMAL_ARRAY );
	SetDefaultRenderStates();

	LOG->Info( "DDR Mini SDL video driver: %s", SDL_GetCurrentVideoDriver() );
	LOG->Info( "DDR Mini GLES vendor: %s", glGetString(GL_VENDOR) );
	LOG->Info( "DDR Mini GLES renderer: %s", glGetString(GL_RENDERER) );
	LOG->Info( "DDR Mini GLES version: %s", glGetString(GL_VERSION) );
	LOG->Info( "DDR Mini GLES max texture size: %d", GetMaxTextureSize() );
	return "";
}

void RageDisplay_DDRMini::Update( float delta )
{
	SDL_PumpEvents();
	SDL_Event event;
	while( SDL_PollEvent(&event) )
		if( event.type == SDL_QUIT ) ExitGame();
}

void RageDisplay_DDRMini::ResolutionChanged()
{
	glViewport( 0, 0, m_Params.width, m_Params.height );
}

const RageDisplay::PixelFormatDesc *RageDisplay_DDRMini::GetPixelFormatDesc( RagePixelFormat pf ) const
{
	ASSERT( pf < NUM_PIX_FORMATS );
	return &g_PixelFormats[pf];
}

bool RageDisplay_DDRMini::BeginFrame()
{
	glViewport( 0, 0, m_Params.width, m_Params.height );
	glClearColor( 0, 0, 0, 1 );
	glDepthMask( GL_TRUE );
	glClear( GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT );
	return true;
}

void RageDisplay_DDRMini::EndFrame()
{
	SDL_GL_SwapWindow( m_Window );
	glFinish();
	ProcessStatsOnFlip();
}

void RageDisplay_DDRMini::SendMatrices()
{
	RageMatrix projection, modelView;
	RageMatrixMultiply( &projection, GetCentering(), GetProjectionTop() );
	RageMatrixMultiply( &modelView, GetViewTop(), GetWorldTop() );
	glMatrixMode( GL_PROJECTION ); glLoadMatrixf( (const float*)&projection );
	glMatrixMode( GL_MODELVIEW ); glLoadMatrixf( (const float*)&modelView );
	glMatrixMode( GL_TEXTURE ); glLoadMatrixf( (const float*)GetTextureTop() );
	glMatrixMode( GL_MODELVIEW );
}

void RageDisplay_DDRMini::Draw( const RageSpriteVertex vertices[], int count, unsigned mode )
{
	if( vertices == NULL || count <= 0 )
		return;

	SendMatrices();
	SetupVertices( vertices, count );
	glDrawArrays( mode, 0, count );
}

void RageDisplay_DDRMini::DrawQuadsInternal( const RageSpriteVertex vertices[], int count )
{
	SendMatrices();
	static vector<unsigned short> indices;
	const int maxQuads = 16384;
	for( int first=0; first+3<count; first += maxQuads*4 )
	{
		int quads = std::min( maxQuads, (count-first)/4 );
		indices.resize( quads*6 );
		for( int q=0; q<quads; ++q )
		{
			unsigned short v = (unsigned short)(q*4);
			indices[q*6+0]=v; indices[q*6+1]=v+1; indices[q*6+2]=v+2;
			indices[q*6+3]=v; indices[q*6+4]=v+2; indices[q*6+5]=v+3;
		}
		SetupVertices( vertices+first, quads*4 );
		glDrawElements( GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, &indices[0] );
	}
}

void RageDisplay_DDRMini::DrawQuadStripInternal( const RageSpriteVertex v[], int n ) { Draw(v,n,GL_TRIANGLE_STRIP); }
void RageDisplay_DDRMini::DrawFanInternal( const RageSpriteVertex v[], int n ) { Draw(v,n,GL_TRIANGLE_FAN); }
void RageDisplay_DDRMini::DrawStripInternal( const RageSpriteVertex v[], int n ) { Draw(v,n,GL_TRIANGLE_STRIP); }
void RageDisplay_DDRMini::DrawTrianglesInternal( const RageSpriteVertex v[], int n ) { Draw(v,n,GL_TRIANGLES); }
void RageDisplay_DDRMini::DrawLineStripInternal( const RageSpriteVertex v[], int n, float width ) { glLineWidth(width); Draw(v,n,GL_LINE_STRIP); }
void RageDisplay_DDRMini::DrawCompiledGeometryInternal( const RageCompiledGeometry *g, int mesh ) { SendMatrices(); g->Draw(mesh); }
RageCompiledGeometry *RageDisplay_DDRMini::CreateCompiledGeometry() { return new RageCompiledGeometryGLES; }
void RageDisplay_DDRMini::DeleteCompiledGeometry( RageCompiledGeometry *g ) { delete g; }

bool RageDisplay_DDRMini::SupportsTextureFormat( RagePixelFormat pf, bool realtime )
{
	GLFormat format;
	return GetGLFormat( pf, format );
}

unsigned RageDisplay_DDRMini::CreateTexture( RagePixelFormat pf, RageSurface *surface, bool mipmaps )
{
	GLFormat format;
	if( !surface || !GetGLFormat(pf, format) ) return 0;
	RageSurface *converted = ConvertUploadSurface( surface, pf );
	RageSurface *upload = converted? converted:surface;
	vector<unsigned char> pixels( upload->w*upload->h*format.bytes );
	for( int row=0; row<upload->h; ++row )
		memcpy( &pixels[row*upload->w*format.bytes], upload->pixels+row*upload->pitch,
			upload->w*format.bytes );

	GLuint texture = 0;
	glGenTextures( 1, &texture );
	glBindTexture( GL_TEXTURE_2D, texture );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexImage2D( GL_TEXTURE_2D, 0, format.format, upload->w, upload->h, 0,
		format.format, format.type, &pixels[0] );
	GLenum error = glGetError();
	delete converted;
	if( error != GL_NO_ERROR )
	{
		LOG->Warn( "DDR Mini GLES texture upload failed: 0x%x", error );
		glDeleteTextures( 1, &texture );
		return 0;
	}
	g_TextureFormats[texture] = pf;
	return texture;
}

void RageDisplay_DDRMini::UpdateTexture( unsigned handle, RageSurface *surface,
	int x, int y, int width, int height )
{
	map<GLuint,RagePixelFormat>::const_iterator it = g_TextureFormats.find(handle);
	if( it == g_TextureFormats.end() || !surface ) return;
	GLFormat format;
	if( !GetGLFormat(it->second, format) ) return;
	RageSurface *converted = ConvertUploadSurface( surface, it->second );
	RageSurface *upload = converted? converted:surface;
	vector<unsigned char> pixels( width*height*format.bytes );
	for( int row=0; row<height; ++row )
		memcpy( &pixels[row*width*format.bytes], upload->pixels+row*upload->pitch,
			width*format.bytes );
	glBindTexture( GL_TEXTURE_2D, handle );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	glTexSubImage2D( GL_TEXTURE_2D, 0, x, y, width, height,
		format.format, format.type, &pixels[0] );
	delete converted;
}

void RageDisplay_DDRMini::DeleteTexture( unsigned handle )
{
	g_TextureFormats.erase( handle );
	GLuint texture = handle;
	if( texture ) glDeleteTextures( 1, &texture );
}

void RageDisplay_DDRMini::ClearAllTextures() { glBindTexture(GL_TEXTURE_2D,0); glDisable(GL_TEXTURE_2D); }
void RageDisplay_DDRMini::SetTexture( int unit, RageTexture *texture )
{
	if( unit != 0 ) return;
	if( texture ) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,texture->GetTexHandle()); }
	else glDisable(GL_TEXTURE_2D);
}
void RageDisplay_DDRMini::SetTextureModeModulate() { glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE); }
void RageDisplay_DDRMini::SetTextureModeGlow() { glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE); glBlendFunc(GL_SRC_ALPHA,GL_ONE); }
void RageDisplay_DDRMini::SetTextureModeAdd() { glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_ADD); }
void RageDisplay_DDRMini::SetTextureWrapping( bool enabled )
{
	GLenum mode=enabled?GL_REPEAT:GL_CLAMP_TO_EDGE;
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,mode); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,mode);
}
int RageDisplay_DDRMini::GetMaxTextureSize() const { GLint size=0; glGetIntegerv(GL_MAX_TEXTURE_SIZE,&size); return size; }
void RageDisplay_DDRMini::SetTextureFiltering( bool enabled )
{
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,enabled?GL_LINEAR:GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,enabled?GL_LINEAR:GL_NEAREST);
}
void RageDisplay_DDRMini::SetBlendMode( BlendMode mode )
{
	glEnable(GL_BLEND);
	switch(mode) {
	case BLEND_NORMAL: glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); break;
	case BLEND_ADD: glBlendFunc(GL_SRC_ALPHA,GL_ONE); break;
	case BLEND_NO_EFFECT: glBlendFunc(GL_ZERO,GL_ONE); break;
	default: ASSERT(0);
	}
}
bool RageDisplay_DDRMini::IsZWriteEnabled() const { GLboolean b=GL_FALSE; glGetBooleanv(GL_DEPTH_WRITEMASK,&b); return b==GL_TRUE; }
bool RageDisplay_DDRMini::IsZTestEnabled() const { GLint f=GL_ALWAYS; glGetIntegerv(GL_DEPTH_FUNC,&f); return f!=GL_ALWAYS; }
void RageDisplay_DDRMini::SetZWrite( bool enabled ) { glDepthMask(enabled?GL_TRUE:GL_FALSE); }
void RageDisplay_DDRMini::SetZBias( float bias ) { glDepthRangef(0.05f*(1-bias),1.0f-0.05f*bias); }
void RageDisplay_DDRMini::SetZTestMode( ZTestMode mode )
{
	glEnable(GL_DEPTH_TEST);
	switch(mode) { case ZTEST_OFF: glDepthFunc(GL_ALWAYS); break; case ZTEST_WRITE_ON_PASS: glDepthFunc(GL_LEQUAL); break; case ZTEST_WRITE_ON_FAIL: glDepthFunc(GL_GREATER); break; }
}
void RageDisplay_DDRMini::ClearZBuffer() { bool w=IsZWriteEnabled(); glDepthMask(GL_TRUE); glClear(GL_DEPTH_BUFFER_BIT); glDepthMask(w?GL_TRUE:GL_FALSE); }
void RageDisplay_DDRMini::SetCullMode( CullMode mode )
{
	if(mode==CULL_NONE) glDisable(GL_CULL_FACE); else { glEnable(GL_CULL_FACE); glCullFace(mode==CULL_BACK?GL_BACK:GL_FRONT); }
}
void RageDisplay_DDRMini::SetAlphaTest( bool enabled ) { if(enabled){glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,0.01f);}else glDisable(GL_ALPHA_TEST); }
void RageDisplay_DDRMini::SetMaterial( const RageColor &e,const RageColor &a,const RageColor &d,const RageColor &s,float sh )
{
	/* Match the established OpenGL backend.  Calling the material entry points
	 * while lighting is disabled is unnecessary, and the DDR Mini's Mali GLES
	 * implementation crashes in that path when gameplay models are drawn. */
	GLboolean lighting = GL_FALSE;
	glGetBooleanv( GL_LIGHTING, &lighting );
	if( lighting )
	{
		glMaterialfv( GL_FRONT_AND_BACK, GL_EMISSION, e );
		glMaterialfv( GL_FRONT_AND_BACK, GL_AMBIENT, a );
		glMaterialfv( GL_FRONT_AND_BACK, GL_DIFFUSE, d );
		glMaterialfv( GL_FRONT_AND_BACK, GL_SPECULAR, s );
		glMaterialf( GL_FRONT_AND_BACK, GL_SHININESS, sh );
	}
	else
	{
		/* Approximate the three lighting terms in RGB, but do not add their
		 * alpha channels.  They have each already been multiplied by the
		 * Actor's diffuse alpha; adding them would turn (for example) the
		 * theme's 0.12 opacity into 0.36 for every model pass. */
		const RageColor color = e + a + d;
		glColor4f( color.r, color.g, color.b, d.a );
	}
}
void RageDisplay_DDRMini::SetLighting( bool enabled ) { if(enabled)glEnable(GL_LIGHTING);else glDisable(GL_LIGHTING); }
void RageDisplay_DDRMini::SetLightOff( int index ) { glDisable(GL_LIGHT0+index); }
void RageDisplay_DDRMini::SetLightDirectional( int index,const RageColor &a,const RageColor &d,const RageColor &s,const RageVector3 &dir )
{
	/* OpenGL transforms a light's position by the current model-view matrix
	 * when glLightfv is called.  This direction is in world space, so do not
	 * let the transform left behind by the current actor rotate it. */
	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	glLoadIdentity();

	glEnable( GL_LIGHT0+index );
	glLightfv( GL_LIGHT0+index, GL_AMBIENT, a );
	glLightfv( GL_LIGHT0+index, GL_DIFFUSE, d );
	glLightfv( GL_LIGHT0+index, GL_SPECULAR, s );
	GLfloat position[4] = { dir.x, dir.y, dir.z, 0 };
	glLightfv( GL_LIGHT0+index, GL_POSITION, position );

	glPopMatrix();
}
void RageDisplay_DDRMini::SetSphereEnvironmentMapping( bool enabled )
{
	/* GLES 1.1 omits desktop OpenGL's sphere-map texture coordinate
	 * generation.  Compiled model geometry generates equivalent coordinates
	 * on the CPU when this flag is active. */
	g_SphereEnvironmentMapping = enabled;
}
void RageDisplay_DDRMini::SetLineWidth( float width ) { glLineWidth(width); }

RageSurface *RageDisplay_DDRMini::CreateScreenshot()
{
	const PixelFormatDesc &desc=g_PixelFormats[FMT_RGB8];
	RageSurface *surface=CreateSurface(m_Params.width,m_Params.height,desc.bpp,desc.masks[0],desc.masks[1],desc.masks[2],0);
	glPixelStorei(GL_PACK_ALIGNMENT,1);
	glReadPixels(0,0,surface->w,surface->h,GL_RGB,GL_UNSIGNED_BYTE,surface->pixels);
	return surface;
}
