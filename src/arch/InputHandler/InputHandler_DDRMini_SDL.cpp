#include "global.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <algorithm>

#include "InputHandler_DDRMini_SDL.h"
#include "RageLog.h"

REGISTER_INPUT_HANDLER2( DDRMiniSDL, DDRMini_SDL );

namespace
{
RageKeySym SDLScancodeToKey( SDL_Scancode key )
{
	if( key >= SDL_SCANCODE_A && key <= SDL_SCANCODE_Z )
		return RageKeySym(KEY_Ca + key - SDL_SCANCODE_A);
	if( key >= SDL_SCANCODE_1 && key <= SDL_SCANCODE_9 )
		return RageKeySym(KEY_C1 + key - SDL_SCANCODE_1);
	if( key >= SDL_SCANCODE_F1 && key <= SDL_SCANCODE_F12 )
		return RageKeySym(KEY_F1 + key - SDL_SCANCODE_F1);
	if( key >= SDL_SCANCODE_KP_1 && key <= SDL_SCANCODE_KP_9 )
		return RageKeySym(KEY_KP_C1 + key - SDL_SCANCODE_KP_1);

	switch( key )
	{
	case SDL_SCANCODE_0: return KEY_C0;
	case SDL_SCANCODE_RETURN: return KEY_ENTER;
	case SDL_SCANCODE_ESCAPE: return KEY_ESC;
	case SDL_SCANCODE_BACKSPACE: return KEY_BACK;
	case SDL_SCANCODE_TAB: return KEY_TAB;
	case SDL_SCANCODE_SPACE: return KEY_SPACE;
	case SDL_SCANCODE_MINUS: return KEY_HYPHEN;
	case SDL_SCANCODE_EQUALS: return KEY_EQUAL;
	case SDL_SCANCODE_LEFTBRACKET: return KEY_LBRACKET;
	case SDL_SCANCODE_RIGHTBRACKET: return KEY_RBRACKET;
	case SDL_SCANCODE_BACKSLASH: return KEY_BACKSLASH;
	case SDL_SCANCODE_SEMICOLON: return KEY_SEMICOLON;
	case SDL_SCANCODE_APOSTROPHE: return KEY_SQUOTE;
	case SDL_SCANCODE_GRAVE: return KEY_ACCENT;
	case SDL_SCANCODE_COMMA: return KEY_COMMA;
	case SDL_SCANCODE_PERIOD: return KEY_PERIOD;
	case SDL_SCANCODE_SLASH: return KEY_SLASH;
	case SDL_SCANCODE_CAPSLOCK: return KEY_CAPSLOCK;
	case SDL_SCANCODE_PRINTSCREEN: return KEY_PRTSC;
	case SDL_SCANCODE_SCROLLLOCK: return KEY_SCRLLOCK;
	case SDL_SCANCODE_PAUSE: return KEY_PAUSE;
	case SDL_SCANCODE_INSERT: return KEY_INSERT;
	case SDL_SCANCODE_HOME: return KEY_HOME;
	case SDL_SCANCODE_PAGEUP: return KEY_PGUP;
	case SDL_SCANCODE_DELETE: return KEY_DEL;
	case SDL_SCANCODE_END: return KEY_END;
	case SDL_SCANCODE_PAGEDOWN: return KEY_PGDN;
	case SDL_SCANCODE_RIGHT: return KEY_RIGHT;
	case SDL_SCANCODE_LEFT: return KEY_LEFT;
	case SDL_SCANCODE_DOWN: return KEY_DOWN;
	case SDL_SCANCODE_UP: return KEY_UP;
	case SDL_SCANCODE_NUMLOCKCLEAR: return KEY_NUMLOCK;
	case SDL_SCANCODE_KP_DIVIDE: return KEY_KP_SLASH;
	case SDL_SCANCODE_KP_MULTIPLY: return KEY_KP_ASTERISK;
	case SDL_SCANCODE_KP_MINUS: return KEY_KP_HYPHEN;
	case SDL_SCANCODE_KP_PLUS: return KEY_KP_PLUS;
	case SDL_SCANCODE_KP_ENTER: return KEY_KP_ENTER;
	case SDL_SCANCODE_KP_0: return KEY_KP_C0;
	case SDL_SCANCODE_KP_PERIOD: return KEY_KP_PERIOD;
	case SDL_SCANCODE_LCTRL: return KEY_LCTRL;
	case SDL_SCANCODE_LSHIFT: return KEY_LSHIFT;
	case SDL_SCANCODE_LALT: return KEY_LALT;
	case SDL_SCANCODE_LGUI: return KEY_LMETA;
	case SDL_SCANCODE_RCTRL: return KEY_RCTRL;
	case SDL_SCANCODE_RSHIFT: return KEY_RSHIFT;
	case SDL_SCANCODE_RALT: return KEY_RALT;
	case SDL_SCANCODE_RGUI: return KEY_RMETA;
	default: return KEY_INVALID;
	}
}
}

InputHandler_DDRMini_SDL::InputHandler_DDRMini_SDL()
{
	m_KeyDown.resize( SDL_NUM_SCANCODES, 0 );
	if( SDL_InitSubSystem(SDL_INIT_EVENTS) != 0 )
		LOG->Warn( "DDR Mini SDL event initialization failed: %s", SDL_GetError() );
	else
		LOG->Info( "DDR Mini SDL keyboard input initialized" );
}

void InputHandler_DDRMini_SDL::SendKey( int scancode, bool down )
{
	if( scancode < 0 || scancode >= (int)m_KeyDown.size() )
		return;
	if( m_KeyDown[scancode] == (down ? 1 : 0) )
		return;

	const RageKeySym key = SDLScancodeToKey( SDL_Scancode(scancode) );
	if( key == KEY_INVALID )
		return;

	m_KeyDown[scancode] = down ? 1 : 0;
	DeviceInput input( DEVICE_KEYBOARD, key );
	input.ts.SetZero();
	ButtonPressed( input, down );
}

void InputHandler_DDRMini_SDL::Update( float fDeltaTime )
{
	SDL_Event event;
	while( SDL_PollEvent(&event) )
	{
		if( event.type == SDL_WINDOWEVENT &&
			event.window.event == SDL_WINDOWEVENT_FOCUS_LOST )
		{
			for( unsigned i=0; i<m_KeyDown.size(); ++i )
				if( m_KeyDown[i] )
					SendKey( i, false );
			continue;
		}
		if( event.type != SDL_KEYDOWN && event.type != SDL_KEYUP )
			continue;
		if( event.type == SDL_KEYDOWN && event.key.repeat )
			continue;

		SendKey( event.key.keysym.scancode, event.type == SDL_KEYDOWN );
	}

	/* Events preserve very short taps, while this state snapshot repairs a
	 * missing event so OpenITG can never retain a phantom held key. */
	int stateCount = 0;
	const Uint8 *state = SDL_GetKeyboardState( &stateCount );
	const int count = std::min( stateCount, (int)m_KeyDown.size() );
	for( int i=0; i<count; ++i )
		if( m_KeyDown[i] != (state[i] ? 1 : 0) )
			SendKey( i, state[i] != 0 );

	InputHandler::UpdateTimer();
}

void InputHandler_DDRMini_SDL::GetDevicesAndDescriptions(
	vector<InputDevice>& devices, vector<CString>& descriptions )
{
	devices.push_back( InputDevice(DEVICE_KEYBOARD) );
	descriptions.push_back( "DDR Mini SDL Keyboard" );
}
