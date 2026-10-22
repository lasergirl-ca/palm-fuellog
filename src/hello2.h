#ifndef HELLO2_H
#define HELLO2_H
#include "sections.h"

/* The section attribute must be on the declaration every caller sees:
 * without it GCC emits a PC-relative call into the main code resource,
 * which jumps to garbage on the device (prc-tools manual, "Breaking up an
 * existing application"). */
const Char *HelloFromSection2(void) SEC2;

#endif
