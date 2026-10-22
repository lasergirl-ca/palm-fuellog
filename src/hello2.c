/* Lives in the second code section (see fuellog.def). Its only job in M0 is
 * to prove that a cross-section call works on the device. */
#include <PalmOS.h>
#include "sections.h"
#include "hello2.h"

const Char *HelloFromSection2(void) SEC2;

const Char *HelloFromSection2(void)
{
    return "called into code section 2";
}
