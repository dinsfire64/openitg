#include "global.h"
#include "snek.h"
#include "RageLog.h"
#include "arch/USB/USBDriver_Impl.h"
#include "RageUtil.h"

const uint16_t SNEK_VENDOR_ID = 0x2E8A;
const uint16_t SNEK_PRODUCT_ID = 0x10A8;

static const int SNEK_INPUT_INTERFACE = 0x01;
static const int SNEK_LIGHTS_INTERFACE = 0x02;

static const int SNEK_GAMEPADREPORTID = 0x01;
static const int SNEK_LIGHTINGREPORTID = 0x01;

// 32 lights, byte per light, with a report id.
static const int iLightingSize = 33;
uint8_t write_snek[iLightingSize];

// 32 bits with a report id
static const int iExpectedNumBytes = 5;
uint8_t read_snek[iExpectedNumBytes] = {0};

// I/O request timeout, in microseconds (so, 10 ms)
const unsigned REQ_TIMEOUT = 10000;

bool snek::DeviceMatches(int iVID, int iPID)
{
	if (iVID != SNEK_VENDOR_ID)
		return false;

	if (iPID == SNEK_PRODUCT_ID)
		return true;

	return false;
}

bool snek::Open()
{
	if (OpenInternal(SNEK_VENDOR_ID, SNEK_PRODUCT_ID))
	{
		m_bInitialized = true;
		return true;
	}
	else
	{
		m_bInitialized = false;
		LOG->Warn("Could not open a connection to the snek device!");
	}

	return false;
}

bool snek::Write(uint32_t data)
{
	// always prepend the data with the report id.
	write_snek[0] = SNEK_LIGHTINGREPORTID;

	for (int i = 0; i < 32; i++)
	{
		// invert the bit orientation as the lighting mapper is all backward for this engine.
		write_snek[iLightingSize - (i + 1)] = IsBitSet(data, i) ? 0xFF : 0x00;
	}

	// libusb can't write direct to the lighting
	// endpoint because it's the wrong direction
	// could fix this in firmware, but oh well. Too late.
	int iResult = m_pDriver->ControlMessage(
		(USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE),
		HID_SET_REPORT,
		HID_IFACE_OUT | SNEK_LIGHTINGREPORTID,
		SNEK_LIGHTS_INTERFACE,
		(char *)&write_snek,
		sizeof(write_snek),
		REQ_TIMEOUT);

	bool bSuccess = true;

	if (!(iResult == iLightingSize))
	{
		LOG->Warn("snek writing failed: %i (%s)",
				  iResult, m_pDriver->GetError());

		bSuccess = false;
	}

	return bSuccess;
}

bool snek::Read(uint32_t *pData)
{
	int iResult = m_pDriver->InterruptRead(SNEK_INPUT_INTERFACE,
										   (char *)read_snek,
										   iExpectedNumBytes,
										   REQ_TIMEOUT);

	bool bSuccess = true;

	if (!(iResult == iExpectedNumBytes))
	{
		LOG->Warn("snek reading failed: %i (%s)",
				  iResult, m_pDriver->GetError());

		bSuccess = false;
	}

	if (read_snek[0] != SNEK_GAMEPADREPORTID)
	{
		LOG->Warn("snek invalid report id: %02x != %02x",
				  read_snek[0], SNEK_GAMEPADREPORTID);

		bSuccess = false;
	}

	// pack the data for the input handler.
	*pData = read_snek[4] << 24 |
			 read_snek[3] << 16 |
			 read_snek[2] << 8 |
			 read_snek[1] << 0;

	return bSuccess;
}
