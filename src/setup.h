/* First-run setup (Welcome, three steps) and the exchange-rate table. */
#ifndef SETUP_H
#define SETUP_H
#include <PalmOS.h>
#include "sections.h"

/* 0 when not in setup; 1 vehicle, 2 units and money (the Prefs form), 3 last service. */
extern UInt8 gSetupStep;

Boolean WelcomeFormHandleEvent(EventType *event) FORMS_SEC;
Boolean RatesFormHandleEvent(EventType *event) FORMS_SEC;

#endif
