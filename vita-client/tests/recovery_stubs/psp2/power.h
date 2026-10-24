#pragma once
#include <stdbool.h>
enum { SCE_POWER_CB_SYSTEM_SUSPEND=0x10000, SCE_POWER_CB_SYSTEM_RESUMING=0x20000,
 SCE_POWER_CB_SYSTEM_RESUME=0x40000, SCE_POWER_CB_APP_RESUME=0x200000 };
int scePowerRegisterCallback(int id);
int scePowerUnregisterCallback(int id);
bool scePowerIsSuspendRequired(void);
