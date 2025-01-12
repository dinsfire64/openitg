#ifndef IO_SNEK_H
#define IO_SNEK_H

#include <stdint.h>
#include "USBDriver.h"

class snek : public USBDriver
{
public:
	static bool DeviceMatches(int iVendorID, int iProductID);
	bool Open();

	bool Read(uint32_t *pData);
	bool Write(uint32_t data);

	bool m_bInitialized;
};

#endif /* IO_SNEK_H */
