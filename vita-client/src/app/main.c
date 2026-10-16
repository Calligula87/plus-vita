// SPDX-License-Identifier: AGPL-3.0-only
#include "ui_vita.h"
#include <psp2/kernel/processmgr.h>

int main(void)
{
	int result = plus_vita_app_run();
	sceKernelExitProcess(result);
	return result;
}
