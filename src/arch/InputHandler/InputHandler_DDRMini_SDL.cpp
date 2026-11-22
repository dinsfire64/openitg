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
	m_Joysticks.resize( NUM_JOYSTICKS );
	if( SDL_InitSubSystem(SDL_INIT_EVENTS | SDL_INIT_JOYSTICK) != 0 )
		LOG->Warn( "DDR Mini SDL event initialization failed: %s", SDL_GetError() );
	else
	{
		SDL_JoystickEventState( SDL_ENABLE );
		LOG->Info( "DDR Mini SDL keyboard and HID pad input initialized" );
		const int count = SDL_NumJoysticks();
		LOG->Info( "DDR Mini SDL found %d joystick device(s)", count );
		for( int i=0; i<count; ++i )
			AddJoystick( i );
	}
}

InputHandler_DDRMini_SDL::~InputHandler_DDRMini_SDL()
{
	for( unsigned i=0; i<m_Joysticks.size(); ++i )
		if( m_Joysticks[i].joystick != NULL )
			SDL_JoystickClose( m_Joysticks[i].joystick );
}

int InputHandler_DDRMini_SDL::FindJoystick( SDL_JoystickID instance ) const
{
	for( unsigned i=0; i<m_Joysticks.size(); ++i )
		if( m_Joysticks[i].joystick != NULL && m_Joysticks[i].instance == instance )
			return i;
	return -1;
}

void InputHandler_DDRMini_SDL::AddJoystick( int deviceIndex )
{
	SDL_JoystickID instance = SDL_JoystickGetDeviceInstanceID( deviceIndex );
	if( instance < 0 || FindJoystick(instance) >= 0 )
		return;

	int slot = -1;
	for( unsigned i=0; i<m_Joysticks.size(); ++i )
		if( m_Joysticks[i].joystick == NULL )
		{
			slot = i;
			break;
		}
	if( slot < 0 )
	{
		LOG->Warn( "DDR Mini SDL has no free joystick slot for device %d", deviceIndex );
		return;
	}

	SDL_Joystick *joystick = SDL_JoystickOpen( deviceIndex );
	if( joystick == NULL )
	{
		LOG->Warn( "DDR Mini SDL could not open joystick %d: %s", deviceIndex, SDL_GetError() );
		return;
	}

	JoystickState &state = m_Joysticks[slot];
	state.joystick = joystick;
	state.instance = SDL_JoystickInstanceID( joystick );
	const char *name = SDL_JoystickName( joystick );
	state.name = name != NULL ? name : "Unknown DDR Mini HID pad";
	std::fill( state.down.begin(), state.down.end(), 0 );
	LOG->Info( "DDR Mini SDL joystick %d: '%s' (%d axes, %d hats, %d buttons)",
		slot+1, state.name.c_str(), SDL_JoystickNumAxes(joystick),
		SDL_JoystickNumHats(joystick), SDL_JoystickNumButtons(joystick) );
}

void InputHandler_DDRMini_SDL::RemoveJoystick( SDL_JoystickID instance )
{
	const int slot = FindJoystick( instance );
	if( slot < 0 )
		return;
	JoystickState &state = m_Joysticks[slot];
	for( int button=0; button<NUM_JOYSTICK_BUTTONS; ++button )
		if( state.down[button] )
			SendJoystickButton( slot, DeviceButton(button), false );
	LOG->Info( "DDR Mini SDL joystick %d removed: '%s'", slot+1, state.name.c_str() );
	SDL_JoystickClose( state.joystick );
	state.joystick = NULL;
	state.instance = -1;
	state.name = "";
}

void InputHandler_DDRMini_SDL::SendJoystickButton( int slot, DeviceButton button, bool down )
{
	if( slot < 0 || slot >= (int)m_Joysticks.size() ||
		button < 0 || button >= NUM_JOYSTICK_BUTTONS )
		return;
	JoystickState &state = m_Joysticks[slot];
	if( state.down[button] == (down ? 1 : 0) )
		return;
	state.down[button] = down ? 1 : 0;
	DeviceInput input( InputDevice(DEVICE_JOY1 + slot), button );
	input.ts.SetZero();
	ButtonPressed( input, down );
}

void InputHandler_DDRMini_SDL::ReconcileJoystick( int slot )
{
	JoystickState &state = m_Joysticks[slot];
	if( state.joystick == NULL )
		return;

	const int buttons = std::min( SDL_JoystickNumButtons(state.joystick), 32 );
	for( int i=0; i<buttons; ++i )
		SendJoystickButton( slot, DeviceButton(JOY_1+i),
			SDL_JoystickGetButton(state.joystick, i) != 0 );

	const int axes = std::min( SDL_JoystickNumAxes(state.joystick), 8 );
	for( int i=0; i<axes; ++i )
	{
		const Sint16 value = SDL_JoystickGetAxis( state.joystick, i );
		SendJoystickButton( slot, DeviceButton(JOY_LEFT+2*i), value < -16000 );
		SendJoystickButton( slot, DeviceButton(JOY_RIGHT+2*i), value > 16000 );
	}

	Uint8 hats = 0;
	for( int i=0; i<SDL_JoystickNumHats(state.joystick); ++i )
		hats |= SDL_JoystickGetHat( state.joystick, i );
	SendJoystickButton( slot, JOY_HAT_LEFT,  (hats & SDL_HAT_LEFT) != 0 );
	SendJoystickButton( slot, JOY_HAT_RIGHT, (hats & SDL_HAT_RIGHT) != 0 );
	SendJoystickButton( slot, JOY_HAT_UP,    (hats & SDL_HAT_UP) != 0 );
	SendJoystickButton( slot, JOY_HAT_DOWN,  (hats & SDL_HAT_DOWN) != 0 );
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
		if( event.type == SDL_JOYDEVICEADDED )
		{
			AddJoystick( event.jdevice.which );
			continue;
		}
		if( event.type == SDL_JOYDEVICEREMOVED )
		{
			RemoveJoystick( event.jdevice.which );
			continue;
		}
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

	SDL_JoystickUpdate();
	for( unsigned i=0; i<m_Joysticks.size(); ++i )
		ReconcileJoystick( i );

	InputHandler::UpdateTimer();
}

void InputHandler_DDRMini_SDL::GetDevicesAndDescriptions(
	vector<InputDevice>& devices, vector<CString>& descriptions )
{
	devices.push_back( InputDevice(DEVICE_KEYBOARD) );
	descriptions.push_back( "DDR Mini SDL Keyboard" );
	for( unsigned i=0; i<m_Joysticks.size(); ++i )
	{
		if( m_Joysticks[i].joystick == NULL )
			continue;
		devices.push_back( InputDevice(DEVICE_JOY1+i) );
		descriptions.push_back( m_Joysticks[i].name );
	}
}
