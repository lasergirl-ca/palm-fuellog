/* Runtime feature checks, resolved once at launch. */
#ifndef DEVICE_H
#define DEVICE_H
#include <PalmOS.h>

void    DeviceInit(void);
Boolean DeviceHasHiRes(void);   /* WinSetCoordinateSystem and double density */
Boolean DeviceHasPIN(void);     /* Pen Input Manager: collapsible input area */
UInt32  DeviceDensity(void);    /* 72 or 144 */

#endif
