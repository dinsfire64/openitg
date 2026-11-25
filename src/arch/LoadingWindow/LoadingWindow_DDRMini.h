#ifndef LOADING_WINDOW_DDRMINI_H
#define LOADING_WINDOW_DDRMINI_H

#include "LoadingWindow.h"

#include <SDL.h>
#include <GLES/gl.h>

class LoadingWindow_DDRMini: public LoadingWindow
{
public:
	LoadingWindow_DDRMini();
	~LoadingWindow_DDRMini();

	CString Init();
	void Paint();
	void SetText( CString str );
	void SetIcon( const RageSurface *pIcon );
	void SetProgress( unsigned iCurrent, unsigned iTarget );

private:
	void Destroy();
	void PumpEvents();
	void SetBanner( const RageSurface *pBanner );
	void DrawQuad( float left, float top, float right, float bottom,
		float red, float green, float blue, float alpha, bool textured = false );
	void DrawText( float centerX, float centerY, float maximumWidth );

	SDL_Window *m_pWindow;
	SDL_GLContext m_Context;
	GLuint m_iIconTexture;
	int m_iIconWidth;
	int m_iIconHeight;
	int m_iTextureWidth;
	int m_iTextureHeight;
	int m_iDrawableWidth;
	int m_iDrawableHeight;
	unsigned m_iCurrent;
	unsigned m_iTarget;
	unsigned m_iAnimationStep;
	bool m_bInitializedVideo;
	CString m_sText;
};

#endif
