#pragma once
enum { SCE_NETCTL_STATE_CONNECTED=3 };
int sceNetCtlInetGetState(int *state);
