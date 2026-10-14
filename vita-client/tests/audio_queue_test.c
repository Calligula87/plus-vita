// SPDX-License-Identifier: AGPL-3.0-only
/* Exercise unchanged production arrival/sequence/queue/output code. Only the
 * external codec, device, clock and log writer are deterministic boundaries. */
#include "audio_playback_vita.h"
#include <opus.h>
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static bool ready, finished, relative_pacing;
static PlusVitaAudioPlayback *audio;
static atomic_uint_fast64_t now_us;
static uint64_t next_arrival, output_deadline;
static unsigned sequence, output_count, log_delay, slow_output, initial_units;
static unsigned first_pcm_delay;
static char final_timing[2048], final_summary[1024];

uint64_t sceKernelGetProcessTimeWide(void) { return atomic_load(&now_us); }
static void deliver_until(uint64_t target)
{
	while(next_arrival <= target && output_count < 5000) {
		atomic_store(&now_us, next_arrival);
		uint8_t packet = 0;
		assert(plus_vita_audio_push(audio, (uint16_t)sequence++, &packet, 1));
		next_arrival += 10000;
	}
	atomic_store(&now_us, target);
}
OpusDecoder *opus_decoder_create(opus_int32 rate, int channels, int *error)
{
	assert(rate == 48000);
	pthread_mutex_lock(&gate);
	while(!ready) pthread_cond_wait(&changed, &gate);
	pthread_mutex_unlock(&gate);
	OpusDecoder *d = malloc(sizeof(*d)); assert(d); d->channels = channels; *error = OPUS_OK; return d;
}
OpusEncoder *opus_encoder_create(opus_int32 rate, int channels, int application, int *error)
{
	assert(rate == 48000 && application == OPUS_APPLICATION_AUDIO);
	OpusEncoder *e = malloc(sizeof(*e)); assert(e); e->channels = channels; *error = OPUS_OK; return e;
}
void opus_decoder_destroy(OpusDecoder *d) { free(d); }
void opus_encoder_destroy(OpusEncoder *e) { free(e); }
int opus_encode(OpusEncoder *e, const int16_t *pcm, int n, unsigned char *packet, opus_int32 cap)
{ (void)e; (void)pcm; assert(n == 480 && cap > 0); *packet = 0; return 1; }
int opus_decode(OpusDecoder *d, const unsigned char *packet, opus_int32 size, int16_t *pcm, int n, int fec)
{ (void)packet; (void)size; assert(n == 480 && !fec); memset(pcm, 0, 480 * d->channels * sizeof(*pcm)); return 480; }
int opus_decoder_ctl(OpusDecoder *d, int request, ...) { (void)d; assert(request == OPUS_RESET_STATE); return OPUS_OK; }
int sceAudioOutOpenPort(int type, int block, int rate, int mode)
{ assert(type == 0 && block == 960 && rate == 48000 && mode == 1); return 1; }
int sceAudioOutSetVolume(int port, int flags, const int *v)
{ assert(port == 1 && flags == 3 && v[0] == 32768 && v[1] == 32768); return 0; }
int sceAudioOutGetConfig(int port, int key) { assert(port == 1); return key == 0 ? 960 : key == 1 ? 48000 : 1; }
int sceAudioOutReleasePort(int port) { assert(port == 1); return 0; }
int sceAudioOutOutput(int port, const void *pcm)
{
	assert(port == 1); if(!pcm) return 0;
	uint64_t now = sceKernelGetProcessTimeWide();
	if(!output_count) output_deadline = now;
	output_deadline += 20000 + slow_output;
	uint64_t target = relative_pacing ? now + 20000 + slow_output : (now > output_deadline ? now : output_deadline);
	deliver_until(target);
	output_count++;
	if(output_count >= 5000) {
		pthread_mutex_lock(&gate); finished = true; pthread_cond_broadcast(&changed); pthread_mutex_unlock(&gate);
	}
	return 0;
}
static void log_message(ChiakiLogLevel level, const char *message, void *user)
{
	(void)level; (void)user;
	if(strstr(message, "audio timing final")) snprintf(final_timing, sizeof(final_timing), "%s", message);
	if(strstr(message, "audio summary")) snprintf(final_summary, sizeof(final_summary), "%s", message);
	if(log_delay && (strstr(message, "PCM levels decoded=") || strstr(message, "audio timing periodic")))
		deliver_until(sceKernelGetProcessTimeWide() + log_delay);
	if(first_pcm_delay && strstr(message, "audio first PCM decoded"))
		deliver_until(sceKernelGetProcessTimeWide() + first_pcm_delay);
}
static unsigned long long field(const char *line, const char *name)
{ const char *p = strstr(line, name); assert(p); return strtoull(p + strlen(name), NULL, 10); }
static uint64_t run(const char *name, unsigned startup, unsigned writer_delay, unsigned pacing_error, bool relative, bool after_first)
{
	ready = finished = false; audio = NULL; sequence = output_count = 0;
	log_delay = writer_delay; slow_output = pacing_error; relative_pacing = relative;
	first_pcm_delay = after_first ? startup : 0;
	if(after_first) startup = 0;
	initial_units = startup ? startup / 10000 : 2;
	atomic_store(&now_us, 1); final_timing[0] = final_summary[0] = 0;
	ChiakiLog log; chiaki_log_init(&log, CHIAKI_LOG_ALL, log_message, NULL);
	ChiakiAudioHeader header = { .channels = 2, .bits = 16, .rate = 48000, .frame_size = 480 };
	audio = plus_vita_audio_start(&header, &log); assert(audio);
	/* Simulate arrivals during native initialization before its consumer runs. */
	for(unsigned i = 0; i < initial_units; i++) {
		atomic_store(&now_us, 1 + (startup ? i * 10000u : 0)); uint8_t packet = 0;
		assert(plus_vita_audio_push(audio, (uint16_t)sequence++, &packet, 1));
	}
	atomic_store(&now_us, 1 + startup);
	output_deadline = sceKernelGetProcessTimeWide(); next_arrival = output_deadline + 10000;
	pthread_mutex_lock(&gate); ready = true; pthread_cond_broadcast(&changed);
	while(!finished) pthread_cond_wait(&changed, &gate);
	pthread_mutex_unlock(&gate);
	assert(plus_vita_audio_stop(audio));
	uint64_t average = field(final_timing, "queue_age_avg_us=");
	printf("%s: queue_avg_us=%llu queue_max_us=%llu gap_stalls=%llu drops=%llu outputs=%llu\n", name,
		(unsigned long long)average, field(final_timing, "queue_age_max_us="), field(final_timing, "output_gap_stalls="),
		field(final_summary, "dropped="), field(final_summary, "outputs="));
	assert(field(final_summary, "failed=") == 0);
	return average;
}
int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	uint64_t clean = run("no startup / no writer delay", 0, 0, 0, false, false);
	uint64_t startup = run("200ms initialization backlog", 200000, 0, 0, false, false);
	uint64_t early = run("200ms writer stall after first dequeue", 200000, 0, 0, false, true);
	uint64_t logging = run("startup + 2x10ms periodic writes / deadline pacing", 200000, 10000, 0, false, false);
	uint64_t relative = run("startup + writes / fixed relative wait", 200000, 10000, 0, true, false);
	uint64_t mismatch = run("startup + 20us output period mismatch", 200000, 0, 20, false, false);
	assert(clean < 15000);
	assert(startup > 175000 && startup < 205000);
	assert(early > 175000 && early < 205000);
	/* The first periodic pair precedes the first hardware output and can
	 * establish an extra 20ms of backlog; later pairs produce transient spikes. */
	assert(logging >= startup && logging - startup < 25000);
	assert(relative > logging + 30000);
	assert(mismatch > startup + 25000);
	puts("audio queue: unchanged production arrival-to-output path separates startup, periodic writer stalls and output pacing; no hardware cause inferred");
}
