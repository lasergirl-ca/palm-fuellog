/* The Service screens: the due list, a service type's intervals, and
 * logging a visit. In their own code section. */
#ifndef SERVICE_FORM_H
#define SERVICE_FORM_H
#include <PalmOS.h>
#include "sections.h"
#include "service.h"

Boolean ServiceFormHandleEvent(EventType *event) SVC_SEC;
Boolean EditServiceFormHandleEvent(EventType *event) SVC_SEC;
Boolean LogServiceFormHandleEvent(EventType *event) SVC_SEC;

/* The most urgent service for a vehicle (255 = every live vehicle): its
 * status, and a line for the fill list such as "Oil and filter due in
 * 640 km" (buf holds 44). svcStatusOK with an empty line when nothing is due. */
UInt8 SvcBanner(UInt8 vehicle, char *buf) SVC_SEC;

#endif
