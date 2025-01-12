#ifndef IO_PIUIOBTN_H
#define IO_PIUIOBTN_H

#include "USBDriver.h"
#include "PIUIO.h"

class PIUIOBTN : public PIUIO
{
public:
	static bool DeviceMatches(int iVendorID, int iProductID);

	bool Open();
};

#endif