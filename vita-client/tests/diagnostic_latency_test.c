// SPDX-License-Identifier: AGPL-3.0-only
#include "diagnostic_log.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

static void *writer(void *unused)
{
	(void)unused;
	for(unsigned int i = 0; i < 200; i++)
		plus_vita_diagnostic_chiaki_log(CHIAKI_LOG_INFO, "Stream probe synthetic test", NULL);
	return NULL;
}

int main(void)
{
	assert(plus_vita_diagnostic_begin());
	plus_vita_diagnostic_stream_timing_begin();
	pthread_t threads[4];
	for(unsigned int i = 0; i < 4; i++) assert(!pthread_create(&threads[i], NULL, writer, NULL));
	for(unsigned int i = 0; i < 4; i++) assert(!pthread_join(threads[i], NULL));
	PlusVitaLatencyStats write_call, mutex_wait;
	plus_vita_diagnostic_stream_timing_end(&write_call, &mutex_wait);
	assert(write_call.count == 800 && mutex_wait.count == 800);
	assert(!write_call.invalid && !mutex_wait.invalid);
	plus_vita_diagnostic_chiaki_log(CHIAKI_LOG_INFO, "Stream probe summary after stop", NULL);
	plus_vita_diagnostic_stream_timing_end(&write_call, &mutex_wait);
	assert(write_call.count == 800);
	plus_vita_diagnostic_stream_timing_begin();
	plus_vita_diagnostic_chiaki_log(CHIAKI_LOG_INFO, "Stream probe access_token=fake-test-only", NULL);
	plus_vita_diagnostic_stream_timing_end(&write_call, &mutex_wait);
	assert(write_call.count == 1 && mutex_wait.count == 1);
	plus_vita_diagnostic_end();
	FILE *file = fopen("plus-vita-allocation.log", "r");
	assert(file);
	char line[512];
	bool redacted = false;
	while(fgets(line, sizeof(line), file))
	{
		assert(!strstr(line, "fake-test-only"));
		if(strstr(line, "sensitive diagnostic entry omitted")) redacted = true;
	}
	fclose(file);
	assert(redacted);
	puts("diagnostic timing: concurrent writers, summary exclusion, reset and redaction passed");
	return 0;
}
