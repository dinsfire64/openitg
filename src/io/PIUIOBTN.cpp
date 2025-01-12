#include "PIUIOBTN.h"

const short PIUIOBTN_VENDOR_ID = 0x0D2F;
const short PIUIOBTN_PRODUCT_ID = 0x1010;

bool PIUIOBTN::DeviceMatches(int iVID, int iPID)
{
	return iVID == PIUIOBTN_VENDOR_ID && iPID == PIUIOBTN_PRODUCT_ID;
}

bool PIUIOBTN::Open()
{
	return OpenInternal(PIUIOBTN_VENDOR_ID, PIUIOBTN_PRODUCT_ID);
}
