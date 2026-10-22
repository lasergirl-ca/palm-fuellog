/* Palm integer types for code that also compiles on the host. */
#ifndef PTYPES_H
#define PTYPES_H
#ifdef __palmos__
#include <PalmTypes.h>
#else
#include <stdint.h>
typedef uint8_t  UInt8;
typedef int8_t   Int8;
typedef uint16_t UInt16;
typedef int16_t  Int16;
typedef uint32_t UInt32;
typedef int32_t  Int32;
#endif
#endif
