// SPDX-License-Identifier: AGPL-3.0-only
#include "../platform/build_identity.h"
#include "../platform/diagnostic_log.h"
#include "../platform/stream_probe.h"

#include <debugScreen.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>

int main(void)
{
	psvDebugScreenInit();
	psvDebugScreenPrintf("PLUS VITA | %s\nReplaying the audited PS3 sample.\n", PLUS_VITA_APP_BUILD);
	/* Settle the initial framebuffer before the replay saves it. */
	sceKernelDelayThread(50000);

	bool ready = plus_vita_diagnostic_begin();
	ChiakiLog log;
	chiaki_log_init(&log, CHIAKI_LOG_INFO | CHIAKI_LOG_WARNING | CHIAKI_LOG_ERROR,
		plus_vita_diagnostic_chiaki_log, NULL);
	plus_vita_diagnostic_event("replay.begin", 0);
	ChiakiErrorCode result = plus_vita_stream_probe_ps3_replay(&log);
	plus_vita_diagnostic_event("replay.complete", result);
	plus_vita_diagnostic_end();
	ready = ready && plus_vita_diagnostic_persistence_ok();
	psvDebugScreenPrintf("\nPS3 replay %s (code %d).\n%s\n\nCIRCLE: Exit\n",
		result == CHIAKI_ERR_SUCCESS ? "passed" : "failed", result,
		ready ? "Log: " PLUS_VITA_PS3_REPLAY_LOG_PATH : "Diagnostic log unavailable.");
	for(;;)
	{
		SceCtrlData pad = { 0 };
		sceKernelDelayThread(20000);
		sceCtrlPeekBufferPositive(0, &pad, 1);
		if(pad.buttons & SCE_CTRL_CIRCLE) break;
	}
	sceKernelExitProcess(result);
	return result;
}
