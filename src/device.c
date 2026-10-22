#include "device.h"
#include <PenInputMgr.h>

static Boolean gHiRes, gPIN;
static UInt32 gDensity = 72;

void DeviceInit(void)
{
    UInt32 v;

    gHiRes = (FtrGet(sysFtrCreator, sysFtrNumWinVersion, &v) == errNone && v >= 4);
    if (gHiRes && WinScreenGetAttribute(winScreenDensity, &v) == errNone)
        gDensity = v;
    gPIN = (FtrGet(pinCreator, pinFtrAPIVersion, &v) == errNone);
}

Boolean DeviceHasHiRes(void) { return gHiRes; }
Boolean DeviceHasPIN(void)   { return gPIN; }
UInt32  DeviceDensity(void)  { return gDensity; }
