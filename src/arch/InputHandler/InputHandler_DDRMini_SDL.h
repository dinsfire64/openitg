#ifndef INPUT_HANDLER_DDRMINI_SDL_H
#define INPUT_HANDLER_DDRMINI_SDL_H

#include "InputHandler.h"

class InputHandler_DDRMini_SDL: public InputHandler
{
public:
	InputHandler_DDRMini_SDL();
	void Update( float fDeltaTime );
	void GetDevicesAndDescriptions( vector<InputDevice>& devices,
		vector<CString>& descriptions );

private:
	vector<unsigned char> m_KeyDown;
	void SendKey( int scancode, bool down );
};

#endif
