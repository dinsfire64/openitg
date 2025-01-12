#ifndef INPUT_HANDLER_PIUIOBTN_H
#define INPUT_HANDLER_PIUIOBTN_H

#include "InputHandler.h"
#include "RageThreads.h"
#include "RageTimer.h"

#include "LightsMapper.h"
#include "io/PIUIOBTN.h"

class InputHandler_PIUIOBTN : public InputHandler
{
public:
	InputHandler_PIUIOBTN();
	~InputHandler_PIUIOBTN();

	void GetDevicesAndDescriptions(vector<InputDevice> &vDevicesOut, vector<CString> &vDescriptionsOut);

private:
	static bool s_bInitialized;

	PIUIOBTN m_pBoard;
	RageThread InputThread;
	LightsMapping m_LightsMappings;

	bool m_bShouldStop;
	uint32_t m_iInputData[4];
	uint32_t m_iLastInputField;
	uint32_t m_iLightsField;

	void Reconnect();

	CString GetInputDescription(const uint64_t iInputData, short iBit);
	void HandleInput();

	void SetLightsMappings();
	void UpdateLights();

	int InputThreadMain();
	static int InputThread_Start(void *data) { return ((InputHandler_PIUIOBTN *)data)->InputThreadMain(); }
};

#endif