#include "diagnostic_log.h"
#include "latency_clock.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#ifdef __vita__
#define PLUS_VITA_DIAGNOSTIC_PATH "ux0:data/plus-vita-allocation.log"
#else
#define PLUS_VITA_DIAGNOSTIC_PATH "plus-vita-allocation.log"
#endif

static FILE *diagnostic_file;
static unsigned long diagnostic_sequence;
static pthread_mutex_t diagnostic_write_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool diagnostic_stream_timing;
static PlusVitaLatencyStats diagnostic_write_call, diagnostic_mutex_wait;

void plus_vita_diagnostic_stream_timing_begin(void)
{
	pthread_mutex_lock(&diagnostic_write_mutex);
	memset(&diagnostic_write_call, 0, sizeof(diagnostic_write_call));
	memset(&diagnostic_mutex_wait, 0, sizeof(diagnostic_mutex_wait));
	diagnostic_stream_timing = true;
	pthread_mutex_unlock(&diagnostic_write_mutex);
}

void plus_vita_diagnostic_stream_timing_end(PlusVitaLatencyStats *write_call,
	PlusVitaLatencyStats *mutex_wait)
{
	pthread_mutex_lock(&diagnostic_write_mutex);
	diagnostic_stream_timing = false;
	*write_call = diagnostic_write_call;
	*mutex_wait = diagnostic_mutex_wait;
	pthread_mutex_unlock(&diagnostic_write_mutex);
}

static bool contains_case_insensitive(const char *text, const char *needle)
{
	if(!text || !needle || !*needle)
		return false;
	for(const char *start = text; *start; start++)
	{
		const char *a = start;
		const char *b = needle;
		while(*a && *b)
		{
			char ca = *a;
			char cb = *b;
			if(ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
			if(cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
			if(ca != cb) break;
			a++;
			b++;
		}
		if(!*b) return true;
	}
	return false;
}

static bool message_is_sensitive(const char *message)
{
	static const char *const forbidden[] = {
		"npsso", "access_token", "refresh_token", "authorization",
		"bearer", "session key", "session_key", "cookie", "jsessionid",
		"launch_spec", "handshake_key"
	};
	for(size_t i = 0; i < sizeof(forbidden) / sizeof(forbidden[0]); i++)
		if(contains_case_insensitive(message, forbidden[i]))
			return true;
	return false;
}

static bool message_is_diagnostic(const char *message)
{
	if(!message) return false;
	return strncmp(message, "Takion ", 7) == 0
		|| strncmp(message, "Senkusha ", 9) == 0
		|| strncmp(message, "Stream probe ", 13) == 0
		|| strncmp(message, "Vita AVC ", 9) == 0
		|| strncmp(message, "GKCrypt ", 8) == 0
		|| strncmp(message, "[PING] ", 7) == 0
		|| strncmp(message, "[CLOUDSESSION] ", 15) == 0
		|| strncmp(message, "[GAIKAI] ping ", 14) == 0
		|| strncmp(message, "[GAIKAI] allocate HTTP status=", 30) == 0
		|| strncmp(message, "[GAIKAI] all datacenter", 25) == 0
		|| strncmp(message, "[GAIKAI] best datacenter", 25) == 0
		|| contains_case_insensitive(message, "network error")
		|| contains_case_insensitive(message, "failed to create")
		|| contains_case_insensitive(message, "failed to resolve");
}

static void write_line(const char *kind, const char *message, int code)
{
	uint64_t start_us = plus_vita_latency_now_us();
	pthread_mutex_lock(&diagnostic_write_mutex);
	uint64_t locked_us = plus_vita_latency_now_us();
	if(!diagnostic_file)
	{
		pthread_mutex_unlock(&diagnostic_write_mutex);
		return;
	}
	diagnostic_sequence++;
	fprintf(diagnostic_file, "%06lu %-8s code=%d %s\n",
		diagnostic_sequence, kind, code, message ? message : "");
	fflush(diagnostic_file);
	if(diagnostic_stream_timing)
	{
		plus_vita_latency_span(&diagnostic_mutex_wait, start_us, locked_us);
		plus_vita_latency_span(&diagnostic_write_call, locked_us, plus_vita_latency_now_us());
	}
	pthread_mutex_unlock(&diagnostic_write_mutex);
}

bool plus_vita_diagnostic_begin(void)
{
	plus_vita_diagnostic_end();
	diagnostic_file = fopen(PLUS_VITA_DIAGNOSTIC_PATH, "w");
	diagnostic_sequence = 0;
	if(!diagnostic_file) return false;
	fputs("plus-vita allocation diagnostic v1\n", diagnostic_file);
	fputs("Secrets and HTTP payloads are intentionally excluded.\n", diagnostic_file);
	fflush(diagnostic_file);
	return true;
}

void plus_vita_diagnostic_event(const char *event, int code)
{
	write_line("EVENT", event, code);
}

void plus_vita_diagnostic_progress(const char *stage)
{
	write_line("STAGE", stage ? stage : "Working", 0);
}

void plus_vita_diagnostic_chiaki_log(ChiakiLogLevel level,
	const char *message, void *user)
{
	(void)user;
	if(!diagnostic_file || !message_is_diagnostic(message)) return;
	if(message_is_sensitive(message))
	{
		write_line("LIB", "[sensitive diagnostic entry omitted]", (int)level);
		return;
	}
	write_line("LIB", message, (int)level);
}

void plus_vita_diagnostic_end(void)
{
	if(!diagnostic_file) return;
	fflush(diagnostic_file);
	fclose(diagnostic_file);
	diagnostic_file = NULL;
}
