// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_RECOVERY_VITA_H
#define PLUS_VITA_RECOVERY_VITA_H
#include <stdbool.h>
typedef enum { PLUS_VITA_RECOVERY_NONE, PLUS_VITA_RECOVERY_WIFI, PLUS_VITA_RECOVERY_SLEEP } PlusVitaRecovery;
/* Main/UI thread only, including the stream loop. Callback work only latches
 * an interruption; resource shutdown remains with the existing owners. */
bool plus_vita_recovery_init(void);
void plus_vita_recovery_fini(void);
PlusVitaRecovery plus_vita_recovery_poll(void);
void plus_vita_recovery_acknowledge(void);
#endif
