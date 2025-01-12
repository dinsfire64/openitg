#include "global.h"

#include "RageLog.h"
#include "InputFilter.h"
#include "DiagnosticsUtil.h"
#include "arch/ArchHooks/ArchHooks.h"
#include "arch/Lights/LightsDriver_External.h"

#include "InputHandler_snek.h"

#define DEVICE_NAME "snek"
#define DEVICE_INPUTDEVICE DEVICE_JOY1 // arbitrary

REGISTER_INPUT_HANDLER(snek);

InputHandler_snek::InputHandler_snek() : m_bShouldStop(false), m_iLastInputField(0)
{
	if (!m_pBoard.Open())
	{
		LOG->Warn("InputHandler_snek: Could not establish a connection with the snek device.");
		return;
	}

	SetLightsMappings();
	DiagnosticsUtil::SetInputType(DEVICE_NAME);

	InputThread.SetName("snek I/O thread");
	InputThread.Create(InputThread_Start, this);
}

InputHandler_snek::~InputHandler_snek()
{
	if (InputThread.IsCreated())
	{
		m_bShouldStop = true;
		InputThread.Wait();
	}
}

void InputHandler_snek::Reconnect()
{
	while (!m_pBoard.Open())
	{
		m_pBoard.Close();
		usleep(1000);
	}
}

void InputHandler_snek::GetDevicesAndDescriptions(vector<InputDevice> &vDevicesOut, vector<CString> &vDescriptionsOut)
{
	if (m_pBoard.m_bInitialized)
	{
		vDevicesOut.push_back(InputDevice(DEVICE_INPUTDEVICE));
		vDescriptionsOut.push_back(DEVICE_NAME);
	}
}

void InputHandler_snek::HandleInput()
{
	m_iInputField = 0;

	// Read input from board
	while (!m_pBoard.Read(&m_iInputField))
		Reconnect();

	// Generate input events bitfield (1 = change, 0 = no change)
	uint32_t iChanged = m_iInputField ^ m_iLastInputField;
	m_iLastInputField = m_iInputField;

	// Construct outside the loop and reassign as needed (it's cheaper).
	DeviceInput di(DEVICE_JOY1, JOY_1);
	RageTimer now;

	for (unsigned iBtn = 0; iBtn < 32; ++iBtn)
	{
		// if this button's status hasn't changed, don't report it.
		if (likely(!IsBitSet(iChanged, iBtn)))
			continue;

		di.button = JOY_1 + iBtn;
		di.ts = now;

		// report this button's status
		ButtonPressed(di, IsBitSet(m_iInputField, iBtn));
	}
}

void InputHandler_snek::SetLightsMappings()
{
	uint32_t iCabinetLights[NUM_CABINET_LIGHTS] =
		{
			/* Upper-left, upper-right, lower-left, lower-right marquee */
			(1 << 7), (1 << 5), (1 << 6), (1 << 4),

			/* P1 select, P2 select, both bass */
			(1 << 2), (1 << 3), (1 << 8), (1 << 8)};

	uint32_t iCustomGameLights[MAX_GAME_CONTROLLERS][MAX_GAME_BUTTONS] =
		{
			/* Left, right, up, down */
			{(1 << 18), (1 << 19), (1 << 16), (1 << 17)}, /* Player 1 */
			{(1 << 26), (1 << 27), (1 << 24), (1 << 25)}, /* Player 2 */
		};

	m_LightsMappings.SetCabinetLights(iCabinetLights);
	m_LightsMappings.SetCustomGameLights(iCustomGameLights);

	// if there are any alternate mappings, set them here now
	LightsMapper::LoadMappings("snek", m_LightsMappings);
}

void InputHandler_snek::UpdateLights()
{
	static const LightsState *ls = LightsDriver_External::Get();

	m_iLightsField = m_LightsMappings.GetLightsField(ls);

	while (!m_pBoard.Write(m_iLightsField))
		Reconnect();
}

int InputHandler_snek::InputThreadMain()
{
	while (!m_bShouldStop)
	{
		HandleInput();
		UpdateLights();
	}

	// Turn off all lights when loop exits
	m_pBoard.Write(0x0);

	return 0;
}
