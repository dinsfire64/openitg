#include "global.h"

#include "RageLog.h"
#include "InputFilter.h"
#include "DiagnosticsUtil.h"
#include "arch/ArchHooks/ArchHooks.h"
#include "arch/Lights/LightsDriver_External.h"

#include "InputHandler_PIUIOBTN.h"
#include "LightsManager.h"

#define DEVICE_NAME "PIU Button Board"
// this is a secondary device, so set it to the second joy
#define DEVICE_INPUTDEVICE DEVICE_JOY2

#define PIUIO_BTN_BITINDEX_START 8

REGISTER_INPUT_HANDLER(PIUIOBTN);

bool InputHandler_PIUIOBTN::s_bInitialized = false;

InputHandler_PIUIOBTN::InputHandler_PIUIOBTN() : m_bShouldStop(false)
{
	if (!m_pBoard.Open())
	{
		LOG->Warn("InputHandler_PIUIOBTN: Could not establish a connection with the PIUBTN device.");
		return;
	}

	SetLightsMappings();
	DiagnosticsUtil::SetInputType(DEVICE_NAME);

	m_iLastInputField = 0;

	InputThread.SetName("PIUBTN I/O thread");
	InputThread.Create(InputThread_Start, this);

	s_bInitialized = true;
}

InputHandler_PIUIOBTN::~InputHandler_PIUIOBTN()
{
	if (InputThread.IsCreated())
	{
		m_bShouldStop = true;
		InputThread.Wait();
	}
}

void InputHandler_PIUIOBTN::Reconnect()
{
	while (!m_pBoard.Open())
	{
		m_pBoard.Close();
		usleep(1000);
	}
}

void InputHandler_PIUIOBTN::GetDevicesAndDescriptions(vector<InputDevice> &vDevicesOut, vector<CString> &vDescriptionsOut)
{
	if (s_bInitialized)
	{
		vDevicesOut.push_back(InputDevice(DEVICE_INPUTDEVICE));
		vDescriptionsOut.push_back(DEVICE_NAME);
	}
}

void InputHandler_PIUIOBTN::HandleInput()
{
	uint32_t m_iInputField = 0;

	// Read input from board
	while (!m_pBoard.Read(&m_iInputData[0]))
		Reconnect();

	// make it so logic is active high.
	m_iInputField = ~m_iInputData[0];

	// generate our input events bit field (1 = change, 0 = no change)
	uint32_t iChanged = m_iInputField ^ m_iLastInputField;
	m_iLastInputField = m_iInputField;

	// Construct outside the loop and reassign as needed (it's cheaper).
	DeviceInput di(DEVICE_INPUTDEVICE, JOY_1);
	RageTimer now;

	// this device only has 8 inputs at the MSB of the word
	// so only iterate through those.
	for (unsigned iBtn = (32 - PIUIO_BTN_BITINDEX_START); iBtn < 32; ++iBtn)
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

void InputHandler_PIUIOBTN::SetLightsMappings()
{
}

void InputHandler_PIUIOBTN::UpdateLights()
{
	static const LightsState *ls = LightsDriver_External::Get();

	m_iLightsField = 0x0;

	// make lights
	// p1 green
	if (ls->m_bCabinetLights[LIGHT_BUTTONS_LEFT])
		m_iLightsField |= (1 << 4);

	// p2 green
	if (ls->m_bCabinetLights[LIGHT_BUTTONS_RIGHT])
		m_iLightsField |= (1 << 0);

	while (!m_pBoard.Write(m_iLightsField))
		Reconnect();
}

int InputHandler_PIUIOBTN::InputThreadMain()
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
