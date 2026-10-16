// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_ACCOUNT_VITA_H
#define PLUS_VITA_ACCOUNT_VITA_H
#include "app.h"
typedef struct plus_vita_account_t PlusVitaAccount;
PlusVitaAccount *plus_vita_account_create(void);
bool plus_vita_account_start(PlusVitaAccount *account, const PlusVitaAppEffect *effect);
/* Poll returns borrowed data. Consume it synchronously, then release_result. */
bool plus_vita_account_poll(PlusVitaAccount *account, PlusVitaAppEvent *event);
void plus_vita_account_release_result(PlusVitaAccount *account);
void plus_vita_account_cancel(PlusVitaAccount *account);
/* False means an in-flight worker retains this object until process exit. */
bool plus_vita_account_close(PlusVitaAccount *account);
#endif
