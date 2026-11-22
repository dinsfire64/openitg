#ifndef RAGE_DISPLAY_DDRMINI_H
#define RAGE_DISPLAY_DDRMINI_H

#include "RageDisplay.h"

struct SDL_Window;
typedef void *SDL_GLContext;

class RageDisplay_DDRMini: public RageDisplay
{
public:
	RageDisplay_DDRMini();
	virtual ~RageDisplay_DDRMini();

	CString Init( VideoModeParams p );
	void Update( float fDeltaTime );
	CString GetApiDescription() const { return "OpenGL ES 1.1 / SDL2"; }
	void ResolutionChanged();
	const PixelFormatDesc *GetPixelFormatDesc( RagePixelFormat pf ) const;

	bool BeginFrame();
	void EndFrame();
	VideoModeParams GetVideoModeParams() const { return m_Params; }
	void SetBlendMode( BlendMode mode );
	bool SupportsTextureFormat( RagePixelFormat pixfmt, bool realtime=false );
	unsigned CreateTexture( RagePixelFormat pixfmt, RageSurface *img, bool bGenerateMipMaps );
	void UpdateTexture( unsigned handle, RageSurface *img, int x, int y, int width, int height );
	void DeleteTexture( unsigned handle );
	void ClearAllTextures();
	int GetNumTextureUnits() { return 1; }
	void SetTexture( int unit, RageTexture *texture );
	void SetTextureModeModulate();
	void SetTextureModeGlow();
	void SetTextureModeAdd();
	void SetTextureWrapping( bool enabled );
	int GetMaxTextureSize() const;
	void SetTextureFiltering( bool enabled );
	bool IsZWriteEnabled() const;
	bool IsZTestEnabled() const;
	void SetZWrite( bool enabled );
	void SetZBias( float bias );
	void SetZTestMode( ZTestMode mode );
	void ClearZBuffer();
	void SetCullMode( CullMode mode );
	void SetAlphaTest( bool enabled );
	void SetMaterial( const RageColor &emissive, const RageColor &ambient,
		const RageColor &diffuse, const RageColor &specular, float shininess );
	void SetLighting( bool enabled );
	void SetLightOff( int index );
	void SetLightDirectional( int index, const RageColor &ambient,
		const RageColor &diffuse, const RageColor &specular, const RageVector3 &dir );
	void SetSphereEnvironmentMapping( bool enabled );
	RageCompiledGeometry *CreateCompiledGeometry();
	void DeleteCompiledGeometry( RageCompiledGeometry *geometry );
	void SetLineWidth( float width );
	RageSurface *CreateScreenshot();

protected:
	void DrawQuadsInternal( const RageSpriteVertex vertices[], int count );
	void DrawQuadStripInternal( const RageSpriteVertex vertices[], int count );
	void DrawFanInternal( const RageSpriteVertex vertices[], int count );
	void DrawStripInternal( const RageSpriteVertex vertices[], int count );
	void DrawTrianglesInternal( const RageSpriteVertex vertices[], int count );
	void DrawCompiledGeometryInternal( const RageCompiledGeometry *geometry, int mesh );
	void DrawLineStripInternal( const RageSpriteVertex vertices[], int count, float width );
	CString TryVideoMode( VideoModeParams params, bool &newDevice );

private:
	void SendMatrices();
	void Draw( const RageSpriteVertex vertices[], int count, unsigned mode );

	SDL_Window *m_Window;
	SDL_GLContext m_Context;
	VideoModeParams m_Params;
};

#endif
