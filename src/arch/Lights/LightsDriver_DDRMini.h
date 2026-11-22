#ifndef LIGHTS_DRIVER_DDRMINI_H
#define LIGHTS_DRIVER_DDRMINI_H

#include <array>
#include <stdint.h>

#include "LightsDriver.h"

class LightsDriver_DDRMini: public LightsDriver
{
public:
	LightsDriver_DDRMini();
	~LightsDriver_DDRMini();
	void Set( const LightsState *ls );

private:
	enum { NUM_LEDS = 12, NUM_PADS = 2, PAD_REPORT_SIZE = 5 };
	std::array<int, NUM_LEDS> m_LEDFDs;
	std::array<int, NUM_PADS> m_PadFDs;
	std::array<uint8_t, NUM_LEDS> m_PreviousLEDs;
	std::array<std::array<uint8_t, PAD_REPORT_SIZE>, NUM_PADS> m_PreviousPads;
	bool m_bFirstUpdate;

	void WriteLED( unsigned index, uint8_t value, bool force );
	void WritePad( unsigned index,
		const std::array<uint8_t, PAD_REPORT_SIZE> &report, bool force );
};

#endif
