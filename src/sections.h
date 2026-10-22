/* Code section attributes. fuellog.def declares the sections; a function
 * placed in one is called from another section through the jump table that
 * m68k-palmos-multigen generates, so no special call syntax is needed. */
#ifndef SECTIONS_H
#define SECTIONS_H
#ifdef __palmos__
#define SEC2       __attribute__((section("sec2")))
#define EXPORT_SEC __attribute__((section("xport")))
#else
#define SEC2
#define EXPORT_SEC
#endif
#endif
