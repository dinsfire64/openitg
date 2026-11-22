#ifndef INPUT_HANDLER_DDRMINI_SDL_H
#define INPUT_HANDLER_DDRMINI_SDL_H

#include <SDL.h>

#include "InputHandler.h"

class InputHandler_DDRMini_SDL: public InputHandler
{
public:
	InputHandler_DDRMini_SDL();
	~InputHandler_DDRMini_SDL();
	void Update( float fDeltaTime );
	void GetDevicesAndDescriptions( vector<InputDevice>& devices,
		vector<CString>& descriptions );

private:
	struct JoystickState
	{
		SDL_Joystick *joystick;
		SDL_JoystickID instance;
		CString name;
		vector<unsigned char> down;
		JoystickState(): joystick(NULL), instance(-1), down(NUM_JOYSTICK_BUTTONS, 0) { }
	};

	vector<unsigned char> m_KeyDown;
	vector<JoystickState> m_Joysticks;
	void SendKey( int scancode, bool down );
	void AddJoystick( int deviceIndex );
	void RemoveJoystick( SDL_JoystickID instance );
	int FindJoystick( SDL_JoystickID instance ) const;
	void SendJoystickButton( int slot, DeviceButton button, bool down );
	void ReconcileJoystick( int slot );
};

#endif
