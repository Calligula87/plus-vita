// SPDX-License-Identifier: AGPL-3.0-only
#include "audio_playback_vita.h"
#include <opus.h>
#include <psp2/audioout.h>
#include <psp2/kernel/processmgr.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_QUEUE 32
#define AUDIO_UNIT_MAX 255
#define AUDIO_BLOCK 960
#define AUDIO_STALL_US ((uint64_t)AUDIO_BLOCK * 1000000u / 48000u + 10000u)
typedef struct { uint16_t sequence; size_t size; uint64_t arrived_us; unsigned local_drops_before; uint8_t data[AUDIO_UNIT_MAX]; } AudioUnit;
struct plus_vita_audio_playback_t {
	ChiakiLog *log;
	OpusDecoder *decoder;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_t thread;
	int port, channels;
	bool stop, failed;
	AudioUnit queue[AUDIO_QUEUE];
	unsigned head, count, peak, dropped, decoded, outputs, concealed, stale;
	bool sequence_valid;
	uint16_t sequence;
	int16_t block[AUDIO_BLOCK * 2];
	unsigned fill;
	unsigned pcm_peak[2], tone_blocks;
	uint64_t pcm_samples[2], pcm_nonzero[2], pcm_square_sum[2];
	/* Arrival counters are updated by the receive callback under mutex. */
	unsigned arrived, arrival_gap_gt20ms, arrival_gap_gt40ms;
	uint64_t last_arrival_us, arrival_gap_max_us;
	/* The remaining timing counters belong to the audio worker. */
	unsigned empty_waits, empty_wait_gt20ms, output_gap_stalls;
	unsigned output_call_stalls, sequence_gaps, sequence_resets, skipped_local;
	unsigned max_missing_units;
	uint64_t empty_wait_max_us, output_gap_max_us, output_call_max_us;
	uint64_t last_output_start_us, queue_age_total_us, queue_age_max_us;
	unsigned queue_age_samples;
};

static void audio_log_timing(PlusVitaAudioPlayback *a, const char *phase)
{
	pthread_mutex_lock(&a->mutex);
	unsigned arrived = a->arrived;
	unsigned arrival_gap_gt20ms = a->arrival_gap_gt20ms;
	unsigned arrival_gap_gt40ms = a->arrival_gap_gt40ms;
	uint64_t arrival_gap_max_us = a->arrival_gap_max_us;
	unsigned queue_count = a->count;
	pthread_mutex_unlock(&a->mutex);
	CHIAKI_LOGI(a->log,
		"Stream probe audio timing %s decoded=%u arrived=%u arrival_gap_max_us=%llu arrival_gt20ms=%u arrival_gt40ms=%u empty_waits=%u empty_wait_max_us=%llu empty_gt20ms=%u output_gap_max_us=%llu output_gap_stalls=%u output_call_max_us=%llu output_call_stalls=%u stall_budget_us=%llu queue_age_avg_us=%llu queue_age_max_us=%llu queue_now=%u seq_gaps=%u seq_resets=%u max_missing=%u skipped_local=%u",
		phase, a->decoded, arrived, (unsigned long long)arrival_gap_max_us,
		arrival_gap_gt20ms, arrival_gap_gt40ms, a->empty_waits,
		(unsigned long long)a->empty_wait_max_us, a->empty_wait_gt20ms,
		(unsigned long long)a->output_gap_max_us, a->output_gap_stalls,
		(unsigned long long)a->output_call_max_us, a->output_call_stalls,
		(unsigned long long)AUDIO_STALL_US,
		(unsigned long long)(a->queue_age_samples ? a->queue_age_total_us / a->queue_age_samples : 0),
		(unsigned long long)a->queue_age_max_us, queue_count, a->sequence_gaps,
		a->sequence_resets, a->max_missing_units, a->skipped_local);
}

static unsigned audio_rms(uint64_t square_sum, uint64_t samples)
{
	uint64_t mean = samples ? square_sum / samples : 0;
	unsigned low = 0, high = 32768;
	while(low < high) {
		unsigned mid = (low + high + 1) / 2;
		if((uint64_t)mid * mid <= mean) low = mid; else high = mid - 1;
	}
	return low;
}

static void audio_measure(PlusVitaAudioPlayback *a, const int16_t *pcm, unsigned samples)
{
	for(unsigned i = 0; i < samples; i++) for(int ch = 0; ch < a->channels; ch++) {
		int value = pcm[i * a->channels + ch];
		unsigned magnitude = (unsigned)(value < 0 ? -value : value);
		if(magnitude > a->pcm_peak[ch]) a->pcm_peak[ch] = magnitude;
		a->pcm_samples[ch]++; if(value) a->pcm_nonzero[ch]++;
		a->pcm_square_sum[ch] += (uint64_t)magnitude * magnitude;
	}
}

static bool audio_output(PlusVitaAudioPlayback *a, const int16_t *pcm, unsigned samples)
{
	while(samples) {
		unsigned take = AUDIO_BLOCK - a->fill;
		if(take > samples) take = samples;
		memcpy(a->block + a->fill * a->channels, pcm, take * a->channels * sizeof(*pcm));
		a->fill += take; pcm += take * a->channels; samples -= take;
		if(a->fill == AUDIO_BLOCK) {
			uint64_t started_us = sceKernelGetProcessTimeWide();
			if(a->last_output_start_us) {
				uint64_t gap_us = started_us - a->last_output_start_us;
				if(gap_us > a->output_gap_max_us) a->output_gap_max_us = gap_us;
				if(gap_us > AUDIO_STALL_US) a->output_gap_stalls++;
			}
			a->last_output_start_us = started_us;
			int result = sceAudioOutOutput(a->port, a->block);
			uint64_t call_us = sceKernelGetProcessTimeWide() - started_us;
			if(call_us > a->output_call_max_us) a->output_call_max_us = call_us;
			if(call_us > AUDIO_STALL_US) a->output_call_stalls++;
			if(result < 0) { CHIAKI_LOGE(a->log, "Stream probe audio output failed=0x%08x", result); return false; }
			a->outputs++; a->fill = 0;
		}
	}
	return true;
}

static void *audio_worker(void *user)
{
	PlusVitaAudioPlayback *a = user;
	CHIAKI_LOGI(a->log, "Stream probe audio worker entered stack=262144; creating decoder");
	int error;
	a->decoder = opus_decoder_create(48000, a->channels, &error);
	if(!a->decoder || error != OPUS_OK) goto failed;
	CHIAKI_LOGI(a->log, "Stream probe audio decoder ready; starting codec selftest");
	OpusEncoder *test_encoder = opus_encoder_create(48000, a->channels, OPUS_APPLICATION_AUDIO, &error);
	if(!test_encoder) goto failed;
	if(error != OPUS_OK) { opus_encoder_destroy(test_encoder); goto failed; }
	int16_t silence[480 * 2] = { 0 }, test_pcm[480 * 2];
	uint8_t test_packet[1276];
	CHIAKI_LOGI(a->log, "Stream probe audio selftest encoder ready; encoding silence");
	int test_size = opus_encode(test_encoder, silence, 480, test_packet, sizeof(test_packet));
	opus_encoder_destroy(test_encoder);
	CHIAKI_LOGI(a->log, "Stream probe audio selftest encoded bytes=%d; decoding", test_size);
	int test_samples = test_size > 0 ? opus_decode(a->decoder, test_packet, test_size, test_pcm, 480, 0) : -1;
	if(test_samples != 480 || opus_decoder_ctl(a->decoder, OPUS_RESET_STATE) != OPUS_OK) goto failed;
	CHIAKI_LOGI(a->log, "Stream probe audio codec selftest passed samples=%d", test_samples);
	a->port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, AUDIO_BLOCK, 48000,
		a->channels == 2 ? SCE_AUDIO_OUT_MODE_STEREO : SCE_AUDIO_OUT_MODE_MONO);
	if(a->port < 0) goto failed;
	int volume[2] = { SCE_AUDIO_OUT_MAX_VOL, SCE_AUDIO_OUT_MAX_VOL };
	if(sceAudioOutSetVolume(a->port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, volume) < 0) goto failed;
	CHIAKI_LOGI(a->log, "Stream probe audio playback started queue=%u block=%u volume=unity port_len=%d port_freq=%d port_mode=%d",
		AUDIO_QUEUE, AUDIO_BLOCK,
		sceAudioOutGetConfig(a->port, SCE_AUDIO_OUT_CONFIG_TYPE_LEN),
		sceAudioOutGetConfig(a->port, SCE_AUDIO_OUT_CONFIG_TYPE_FREQ),
		sceAudioOutGetConfig(a->port, SCE_AUDIO_OUT_CONFIG_TYPE_MODE));
	/* The former 640ms reference tone blocked live packet consumption while
	 * only 160ms fit in the queue. Normal streaming starts decoding at once. */
	CHIAKI_LOGI(a->log, "Stream probe audio reference tone disabled for live playback");
	int16_t pcm[480 * 2];
	unsigned pending_local_drops = 0;
	for(;;) {
		pthread_mutex_lock(&a->mutex);
		if(!a->stop && !a->count) {
			uint64_t wait_started_us = sceKernelGetProcessTimeWide();
			while(!a->stop && !a->count) pthread_cond_wait(&a->cond, &a->mutex);
			if(!a->stop && a->decoded) {
				uint64_t wait_us = sceKernelGetProcessTimeWide() - wait_started_us;
				a->empty_waits++;
				if(wait_us > a->empty_wait_max_us) a->empty_wait_max_us = wait_us;
				if(wait_us > 20000) a->empty_wait_gt20ms++;
			}
		}
		if(a->stop) { pthread_mutex_unlock(&a->mutex); break; }
		AudioUnit unit = a->queue[a->head];
		a->head = (a->head + 1) % AUDIO_QUEUE; a->count--;
		unsigned queue_after_pop = a->count, dropped_total = a->dropped;
		pthread_mutex_unlock(&a->mutex);
		uint64_t queue_age_us = sceKernelGetProcessTimeWide() - unit.arrived_us;
		a->queue_age_total_us += queue_age_us;
		a->queue_age_samples++;
		if(queue_age_us > a->queue_age_max_us) a->queue_age_max_us = queue_age_us;
		pending_local_drops += unit.local_drops_before;
		uint16_t delta = (uint16_t)(unit.sequence - a->sequence);
		if(a->sequence_valid && (!delta || delta >= 0x8000)) { a->stale++; continue; }
		if(a->sequence_valid && delta > 1) {
			a->sequence_gaps++;
			if((unsigned)(delta - 1) > a->max_missing_units) a->max_missing_units = (unsigned)(delta - 1);
			/* A full local queue deliberately evicts old units. Concealing those
			 * units would spend another 10ms per eviction and cause a drop/PLC
			 * feedback loop. Only conceal units actually missing on the wire. */
			unsigned missing = (unsigned)(delta - 1);
			unsigned local = pending_local_drops < missing ? pending_local_drops : missing;
			unsigned wire_missing = missing - local;
			a->skipped_local += local;
			if(wire_missing <= 3) {
				for(unsigned i = 0; i < wire_missing; i++) {
					int n = opus_decode(a->decoder, NULL, 0, pcm, 480, 0);
					if(n != 480 || !audio_output(a, pcm, (unsigned)n)) goto failed;
					a->concealed++;
				}
			} else { a->sequence_resets++; opus_decoder_ctl(a->decoder, OPUS_RESET_STATE); a->fill = 0; }
		}
		pending_local_drops = 0;
		int n = opus_decode(a->decoder, unit.data, (opus_int32)unit.size, pcm, 480, 0);
		if(n != 480) { CHIAKI_LOGE(a->log, "Stream probe audio decode failed samples=%d", n); goto failed; }
		a->decoded++; a->sequence = unit.sequence; a->sequence_valid = true;
		audio_measure(a, pcm, (unsigned)n);
		if(a->decoded == 1 || a->decoded % 1000 == 0) {
			CHIAKI_LOGI(a->log, "Stream probe audio PCM levels decoded=%u peak_L=%u peak_R=%u rms_L=%u rms_R=%u nonzero_L=%llu nonzero_R=%llu queue=%u dropped=%u concealed=%u stale=%u",
				a->decoded, a->pcm_peak[0], a->pcm_peak[1],
				audio_rms(a->pcm_square_sum[0], a->pcm_samples[0]), audio_rms(a->pcm_square_sum[1], a->pcm_samples[1]),
				(unsigned long long)a->pcm_nonzero[0], (unsigned long long)a->pcm_nonzero[1],
				queue_after_pop, dropped_total, a->concealed, a->stale);
			audio_log_timing(a, "periodic");
		}
		if(!audio_output(a, pcm, (unsigned)n)) goto failed;
		if(a->decoded == 1) CHIAKI_LOGI(a->log, "Stream probe audio first PCM decoded samples=%d", n);
	}
	return NULL;
failed:
	CHIAKI_LOGE(a->log, "Stream probe audio worker failed; initialization or playback stopped");
	pthread_mutex_lock(&a->mutex); a->failed = true; pthread_mutex_unlock(&a->mutex);
	return NULL;
}

PlusVitaAudioPlayback *plus_vita_audio_start(const ChiakiAudioHeader *h, ChiakiLog *log)
{
	CHIAKI_LOGI(log, "Stream probe audio header channels=%u rate=%u bits=%u samples=%u", h->channels, h->rate, h->bits, h->frame_size);
	if(h->rate != 48000 || h->bits != 16 || h->frame_size != 480 || (h->channels != 1 && h->channels != 2)) return NULL;
	PlusVitaAudioPlayback *a = calloc(1, sizeof(*a));
	if(!a) return NULL;
	a->log = log; a->channels = h->channels; a->port = -1;
	/* Receive callback only allocates the bounded queue and launches worker.
	 * Opus and audio-port initialization never run on its default stack. */
	if(pthread_mutex_init(&a->mutex, NULL)) goto free_audio;
	if(pthread_cond_init(&a->cond, NULL)) goto free_mutex;
	pthread_attr_t attr;
	if(pthread_attr_init(&attr)) goto free_cond;
	int result = pthread_attr_setstacksize(&attr, 256 * 1024);
	if(!result) result = pthread_create(&a->thread, &attr, audio_worker, a);
	pthread_attr_destroy(&attr);
	if(result) goto free_cond;
	CHIAKI_LOGI(log, "Stream probe audio worker launched; initialization asynchronous");
	return a;
free_cond: pthread_cond_destroy(&a->cond);
free_mutex: pthread_mutex_destroy(&a->mutex);
free_audio: free(a); return NULL;
}

bool plus_vita_audio_push(PlusVitaAudioPlayback *a, uint16_t sequence, const uint8_t *data, size_t size)
{
	if(!a || !size || size > AUDIO_UNIT_MAX) return false;
	uint64_t arrived_us = sceKernelGetProcessTimeWide();
	pthread_mutex_lock(&a->mutex);
	if(a->failed || a->stop) { pthread_mutex_unlock(&a->mutex); return false; }
	if(a->last_arrival_us) {
		uint64_t gap_us = arrived_us - a->last_arrival_us;
		if(gap_us > a->arrival_gap_max_us) a->arrival_gap_max_us = gap_us;
		if(gap_us > 20000) a->arrival_gap_gt20ms++;
		if(gap_us > 40000) a->arrival_gap_gt40ms++;
	}
	a->last_arrival_us = arrived_us;
	a->arrived++;
	if(a->count == AUDIO_QUEUE) {
		unsigned carry = a->queue[a->head].local_drops_before + 1;
		a->head = (a->head + 1) % AUDIO_QUEUE; a->count--; a->dropped++;
		a->queue[a->head].local_drops_before += carry;
	}
	AudioUnit *unit = &a->queue[(a->head + a->count) % AUDIO_QUEUE];
	unit->sequence = sequence; unit->size = size; unit->arrived_us = arrived_us;
	unit->local_drops_before = 0; memcpy(unit->data, data, size);
	a->count++; if(a->count > a->peak) a->peak = a->count;
	pthread_cond_signal(&a->cond); pthread_mutex_unlock(&a->mutex); return true;
}

bool plus_vita_audio_stop(PlusVitaAudioPlayback *a)
{
	if(!a) return true;
	pthread_mutex_lock(&a->mutex); a->stop = true; pthread_cond_signal(&a->cond); pthread_mutex_unlock(&a->mutex);
	pthread_join(a->thread, NULL);
	int drain = a->port >= 0 ? sceAudioOutOutput(a->port, NULL) : 0;
	int release = a->port >= 0 ? sceAudioOutReleasePort(a->port) : 0;
	CHIAKI_LOGI(a->log, "Stream probe audio PCM final peak_L=%u peak_R=%u rms_L=%u rms_R=%u nonzero_L=%llu nonzero_R=%llu tone_blocks=%u",
		a->pcm_peak[0], a->pcm_peak[1], audio_rms(a->pcm_square_sum[0], a->pcm_samples[0]),
		audio_rms(a->pcm_square_sum[1], a->pcm_samples[1]),
		(unsigned long long)a->pcm_nonzero[0], (unsigned long long)a->pcm_nonzero[1], a->tone_blocks);
	CHIAKI_LOGI(a->log, "Stream probe audio summary decoded=%u outputs=%u concealed=%u stale=%u dropped=%u queue_peak=%u failed=%d drain=0x%08x release=0x%08x",
		a->decoded, a->outputs, a->concealed, a->stale, a->dropped, a->peak, a->failed, drain, release);
	audio_log_timing(a, "final");
	pthread_cond_destroy(&a->cond); pthread_mutex_destroy(&a->mutex);
	bool success = !a->failed && a->outputs > 0 && drain >= 0 && release >= 0;
	opus_decoder_destroy(a->decoder);
	free(a); return success;
}
