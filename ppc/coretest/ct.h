/* Included first in every suite of the core self-test: main is renamed per
 * suite and printf collected by the Makefile's -D flags; this declares where
 * the output goes. */
#ifndef FUELLOG_CT_H
#define FUELLOG_CT_H
int ct_printf(const char *fmt, ...);
#endif
