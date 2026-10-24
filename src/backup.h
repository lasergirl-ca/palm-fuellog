/* Backup sets on the card and restore. Device only, in the io section. */
#ifndef BACKUP_H
#define BACKUP_H
#include <PalmOS.h>
#include "sections.h"
#include "card.h"
Err     BackupNow(UInt16 vol, char *setOut) IO_SEC;         /* setOut (32): "20261024-0730" */
UInt16  BackupList(UInt16 vol, char names[][cardNameLen], UInt16 max) IO_SEC;
Err     BackupRestore(UInt16 vol, const char *set) IO_SEC;  /* dmErrAlreadyExists stands for "newer schema" */
Boolean BackupDue(void) IO_SEC;                             /* auto backup on and a week since the last */
void    BackupAuto(void) IO_SEC;                            /* silent backup on exit when due and a card is in */
#endif
