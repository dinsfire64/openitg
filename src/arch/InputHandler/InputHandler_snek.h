#ifndef INPUT_HANDLER_SNEK_H
#define INPUT_HANDLER_SNEK_H

#include "InputHandler.h"
#include "RageThreads.h"
#include "RageTimer.h"

#include "LightsMapper.h"
#include "io/snek.h"

class InputHandler_snek : public InputHandler
{
public:
	InputHandler_snek();
	~InputHandler_snek();

	void GetDevicesAndDescriptions(vector<InputDevice> &vDevicesOut, vector<CString> &vDescriptionsOut);

private:
	snek m_pBoard;
	RageThread InputThread;
	LightsMapping m_LightsMappings;

	bool m_bShouldStop;
	uint32_t m_iInputField;
	uint32_t m_iLastInputField;
	uint64_t m_iLightsField;

	void Reconnect();

	CString GetInputDescription(const uint64_t iInputData, short iBit);
	void HandleInput();

	void SetLightsMappings();
	void UpdateLights();

	int InputThreadMain();
	static int InputThread_Start(void *data) { return ((InputHandler_snek *)data)->InputThreadMain(); }
};

#endif