// SPDX-License-Identifier: AGPL-3.0-only

#include "stream_probe.h"
#include "h264_config.h"
#include "video_decoder_vita.h"
#include "video_present_vita.h"
#include "audio_playback_vita.h"
#include "video_fec.h"
#include "video_reorder.h"
#include "diagnostic_log.h"
#include "feedback_latency.h"
#include "input_vita.h"
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/kernel/processmgr.h>

#include <chiaki/base64.h>
#include <chiaki/audio.h>
#include <chiaki/bitstream.h>
#include <chiaki/ecdh.h>
#include <chiaki/feedbacksender.h>
#include <chiaki/gkcrypt.h>
#include <chiaki/session.h>
#include <chiaki/takion.h>
#include <chiaki/video.h>

#include "pb_decode.h"
#include "pb_encode.h"
#include "pb_utils.h"
#include "takion.pb.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <mbedtls/md.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define PROBE_TIMEOUT_MS 15000
#define PROBE_VIDEO_HEADER_MAX 2048
#define PROBE_VIDEO_UNITS_MAX 512
#define PROBE_DECODE_ATTEMPTS_MAX 32
#if PLUS_VITA_DIAGNOSTIC_APP
#define PROBE_PREVIEW_MS 600000u
#else
/* The graphical player is user-stopped; retain the old bounded diagnostic
 * profile separately as a rollback and comparison artifact. */
#define PROBE_PREVIEW_MS 0u
#endif
#define PROBE_STOP_HOLD_MS 2000
#define PROBE_RECOVERY_FAULT_TEST 0
#define PROBE_QUEUE_MAX 16
#define PROBE_FRAME_BYTES_MAX (1024u * 1024u)
#define PROBE_CAPTURE_BYTES_MAX (8u * 1024u * 1024u)
#define PROBE_CAPTURE_MS 5000u
#define PROBE_CAPTURE_ONLY 0

typedef struct {
	uint8_t *data;
	size_t size;
	bool include_header;
	unsigned int recovery_generation;
	uint64_t enqueued_us;
	uint64_t first_packet_us, ready_us;
} ProbeQueuedFrame;

typedef struct probe_video_assembly_t
{
	bool active;
	ChiakiSeqNum16 frame_index;
	uint16_t units_total;
	uint16_t units_fec;
	uint16_t units_source;
	uint16_t units_received;
	uint16_t parity_received;
	bool fec_attempted;
	bool late_received;
	size_t stored_bytes;
	uint64_t first_packet_us, ready_us;
	uint8_t *unit_data[PROBE_VIDEO_UNITS_MAX];
	size_t unit_size[PROBE_VIDEO_UNITS_MAX];
} ProbeVideoAssembly;

typedef struct {
	unsigned int profile;
	uint16_t expected, received;
	bool units[PROBE_VIDEO_UNITS_MAX];
} ProbeReceptionFrame;

typedef struct {
	PlusVitaLatencyStats input_poll_gap, input_publish_call, main_mutex_wait;
	PlusVitaLatencyStats callback_mutex_wait, callback_work;
	PlusVitaLatencyStats frame_assembly, ready_to_enqueue, queue_wait;
	PlusVitaLatencyStats decode_prepare, decode, display_wait;
	PlusVitaLatencyStats submitted_first_packet_to_present, submitted_ready_to_present;
	PlusVitaLatencyStats log_write_call, log_mutex_wait;
	PlusVitaFeedbackLatency feedback;
	uint64_t input_poll_failures;
} ProbeLatency;

typedef enum plus_vita_stream_probe_state_t
{
	PROBE_CONNECTING,
	PROBE_EXPECT_BANG,
	PROBE_EXPECT_STREAMINFO,
	PROBE_EXPECT_MEDIA,
	PROBE_COMPLETE,
	PROBE_FAILED,
} PlusVitaStreamProbeState;

typedef struct plus_vita_stream_probe_t
{
	ChiakiLog *log;
	ChiakiTakion takion;
	ChiakiECDH ecdh;
	ChiakiGKCrypt *crypt_local;
	ChiakiGKCrypt *crypt_remote;
	ChiakiMutex mutex;
	ChiakiCond cond;
	PlusVitaStreamProbeState state;
	uint8_t handshake_key[CHIAKI_HANDSHAKE_KEY_SIZE];
	const ChiakiCloudProvisionResult *provision;
	unsigned int video_packets;
	unsigned int audio_packets;
	unsigned int video_frames;
	unsigned int fec_frames_advertised;
	unsigned int fec_packets_seen;
	unsigned int fec_attempts, fec_repaired_frames, fec_repaired_units, fec_failed;
	uint64_t fec_total_us, fec_max_us;
	unsigned int audio_frames;
	PlusVitaAudioPlayback *audio_playback;
	bool video_keyframe;
	PlusVitaH264Guard reference_guard;
	bool awaiting_recovery_idr;
	uint64_t recovery_started_ms;
	uint64_t recovery_last_request_ms;
	ChiakiSeqNum16 recovery_report_start;
	unsigned int recovery_requests;
	unsigned int recovery_count, recovery_dropped;
	unsigned int recovery_resumes;
	uint64_t recovery_resumed_ms;
	bool recovery_output_pending;
	bool recovery_fault_injected;
	bool video_frame_completed_valid;
	ChiakiSeqNum16 video_frame_completed;
	ChiakiBitstream bitstream;
	bool bitstream_ready;
	ChiakiCodec video_codec;
	uint8_t video_header[PROBE_VIDEO_HEADER_MAX];
	size_t video_header_size;
	uint8_t original_video_header[PROBE_VIDEO_HEADER_MAX];
	size_t original_video_header_size;
	uint32_t video_width;
	uint32_t video_height;
	unsigned int selected_profile_index;
	unsigned int profile_packet_counts[8];
	unsigned int last_profile_index;
	bool last_profile_valid;
	/* Wire reception counts, independent of the selected decoder profile. */
	PlusVitaVideoReorder feedback_window;
	ProbeReceptionFrame feedback_frames[PLUS_VITA_VIDEO_REORDER_SLOTS];
	uint32_t feedback_pending_received;
	uint32_t feedback_pending_lost;
	unsigned int feedback_reports;
	unsigned int quality_reports;
	PlusVitaVideoDecoder video_decoder;
	bool video_decoder_ready;
	unsigned int video_decode_attempts;
	bool video_decoded_frame;
	ProbeQueuedFrame queue[PROBE_QUEUE_MAX];
	unsigned int queue_head;
	unsigned int queue_count;
	unsigned int queue_peak;
	unsigned int incomplete_frames;
	unsigned int stale_packets;
	ProbeVideoAssembly video_assembly[PLUS_VITA_VIDEO_REORDER_SLOTS];
	PlusVitaVideoReorder video_reorder;
	unsigned int reorder_late_packets, reorder_saved_frames, reorder_expired_frames;
	unsigned int reorder_missing_frames, reorder_pressure_drops, reorder_peak;
	unsigned int reorder_retired_packets;
	unsigned int reorder_waits;
	uint64_t reorder_wait_total_us, reorder_wait_max_us;
	uint64_t reorder_held_total_us, reorder_held_max_us;
	uint8_t *capture_data;
	size_t capture_size;
	unsigned int capture_frames;
	uint64_t capture_started;
	bool capture_done;
	bool capture_gap;
	ChiakiSeqNum16 capture_last_frame;
	bool latency_collecting;
	uint64_t callback_arrival_us;
	ProbeLatency latency;
} PlusVitaStreamProbe;

static void probe_latency_log(ChiakiLog *log, const char *name,
	const PlusVitaLatencyStats *stats)
{
	CHIAKI_LOGI(log,
		"Stream probe latency metric=%s count=%llu mean_us=%llu p50_upper_us=%llu p95_upper_us=%llu p99_upper_us=%llu max_us=%llu invalid=%llu",
		name, (unsigned long long)stats->count,
		(unsigned long long)(stats->count ? stats->total_us / stats->count : 0),
		(unsigned long long)plus_vita_latency_percentile_upper(stats, 50),
		(unsigned long long)plus_vita_latency_percentile_upper(stats, 95),
		(unsigned long long)plus_vita_latency_percentile_upper(stats, 99),
		(unsigned long long)stats->max_us, (unsigned long long)stats->invalid);
}

/* Dump only after receive and feedback workers have stopped. Each histogram
 * has one writer or uses an existing mutex; no per-packet logging is added. */
static void probe_latency_dump(PlusVitaStreamProbe *probe)
{
	ProbeLatency *p = &probe->latency;
	CHIAKI_LOGI(probe->log,
		"Stream probe latency summary: version=1 mode=" PLUS_VITA_STREAM_PROBE_MODE " poll_failures=%llu published=%llu consumed=%llu coalesced=%llu send_failures=%llu units=us percentiles=bucket-upper local-only=1",
		(unsigned long long)p->input_poll_failures,
		(unsigned long long)p->feedback.published, (unsigned long long)p->feedback.consumed,
		(unsigned long long)p->feedback.coalesced, (unsigned long long)p->feedback.send_failures);
	CHIAKI_LOGI(probe->log, "Stream probe sender diagnostic summary: suppressed_logs=%llu",
		(unsigned long long)p->feedback.suppressed_sender_logs);
#define PROBE_LATENCY_LOG(field) probe_latency_log(probe->log, #field, &p->field)
	PROBE_LATENCY_LOG(input_poll_gap);
	PROBE_LATENCY_LOG(input_publish_call);
	PROBE_LATENCY_LOG(main_mutex_wait);
	PROBE_LATENCY_LOG(callback_mutex_wait);
	PROBE_LATENCY_LOG(callback_work);
	PROBE_LATENCY_LOG(frame_assembly);
	PROBE_LATENCY_LOG(ready_to_enqueue);
	PROBE_LATENCY_LOG(queue_wait);
	PROBE_LATENCY_LOG(decode_prepare);
	PROBE_LATENCY_LOG(decode);
	PROBE_LATENCY_LOG(display_wait);
	PROBE_LATENCY_LOG(submitted_first_packet_to_present);
	PROBE_LATENCY_LOG(submitted_ready_to_present);
	PROBE_LATENCY_LOG(log_write_call);
	PROBE_LATENCY_LOG(log_mutex_wait);
	PROBE_LATENCY_LOG(feedback.sample_to_publish);
	PROBE_LATENCY_LOG(feedback.publish_to_wake);
	PROBE_LATENCY_LOG(feedback.sample_to_state_send);
	PROBE_LATENCY_LOG(feedback.sample_to_history_send);
	PROBE_LATENCY_LOG(feedback.state_send_call);
	PROBE_LATENCY_LOG(feedback.history_send_call);
#undef PROBE_LATENCY_LOG
}

/* Main preview thread only. The receiver has its own mutex-wait metric. */
static void probe_preview_lock(PlusVitaStreamProbe *probe)
{
	uint64_t start_us = sceKernelGetProcessTimeWide();
	chiaki_mutex_lock(&probe->mutex);
	if(probe->latency_collecting)
		plus_vita_latency_span(&probe->latency.main_mutex_wait, start_us,
			sceKernelGetProcessTimeWide());
}

typedef struct probe_streaminfo_context_t
{
	ChiakiLog *log;
	unsigned int profiles;
	bool profiles_valid;
	bool selected_profile_valid;
	unsigned int selected_profile_index;
	uint8_t video_header[PROBE_VIDEO_HEADER_MAX];
	size_t video_header_size;
	uint32_t video_width;
	uint32_t video_height;
} ProbeStreamInfoContext;

static bool encode_zero_key(pb_ostream_t *stream, const pb_field_t *field,
	void *const *arg)
{
	(void)arg;
	if(!pb_encode_tag_for_field(stream, field))
		return false;
	const uint8_t zero[4] = { 0 };
	return pb_encode_string(stream, zero, sizeof(zero));
}

static bool probe_decode_resolution(pb_istream_t *stream,
	const pb_field_t *field, void **arg)
{
	(void)field;
	ProbeStreamInfoContext *ctx = *arg;
	tkproto_ResolutionPayload resolution = { 0 };
	uint8_t profile_header[PROBE_VIDEO_HEADER_MAX];
	ChiakiPBDecodeBuf header = {
		.max_size = PROBE_VIDEO_HEADER_MAX, .size = 0,
		.buf = profile_header };
	resolution.video_header.arg = &header;
	resolution.video_header.funcs.decode = chiaki_pb_decode_buf;
	if(!pb_decode(stream, tkproto_ResolutionPayload_fields, &resolution))
		return false;
	PlusVitaH264Config h264 = { 0 };
	bool h264_valid = plus_vita_h264_config(header.buf, header.size, &h264);
	if(h264_valid)
		CHIAKI_LOGI(ctx->log,
			"Stream probe profile[%u]: %ux%u header=%lu H264 profile=%u level=%u coded=%ux%u refs=%u buffered=%u restriction=%d",
			ctx->profiles, resolution.width, resolution.height,
			(unsigned long)header.size, h264.profile, h264.level,
			h264.coded_width, h264.coded_height,
			h264.max_ref_frames, h264.max_buffered_frames,
			h264.has_buffer_restriction ? 1 : 0);
	else
		CHIAKI_LOGI(ctx->log,
			"Stream probe profile[%u]: %ux%u header=%lu H264 SPS unavailable",
			ctx->profiles, resolution.width, resolution.height,
			(unsigned long)header.size);
	if(h264_valid && resolution.width && resolution.height
		&& resolution.width == 1280 && resolution.height == 720
		&& (!ctx->selected_profile_valid
			|| resolution.width * resolution.height
				> ctx->video_width * ctx->video_height))
	{
		ctx->selected_profile_valid = true;
		ctx->selected_profile_index = ctx->profiles;
		ctx->video_header_size = header.size;
		ctx->video_width = resolution.width;
		ctx->video_height = resolution.height;
		memcpy(ctx->video_header, header.buf, header.size);
	}
	ctx->profiles++;
	if(!resolution.width || !resolution.height || !header.size)
		ctx->profiles_valid = false;
	return true;
}

static void probe_video_assembly_reset(ProbeVideoAssembly *assembly)
{
	for(unsigned int i = 0; i < PROBE_VIDEO_UNITS_MAX; i++)
		free(assembly->unit_data[i]);
	memset(assembly, 0, sizeof(*assembly));
}

/* Only Annex-B NAL type 5 starts the capture; an ordinary I slice is not IDR. */
static bool probe_has_nal(const uint8_t *data, size_t size, uint8_t type)
{
	for(size_t i = 0; i + 3 < size; i++)
	{
		if(data[i] || data[i + 1]) continue;
		size_t nal = data[i + 2] == 1 ? i + 3
			: (i + 4 < size && !data[i + 2] && data[i + 3] == 1 ? i + 4 : size);
		if(nal < size && (data[nal] & 0x1f) == type) return true;
	}
	return false;
}

static void probe_capture_frame(PlusVitaStreamProbe *probe,
	const uint8_t *frame, size_t size, ChiakiSeqNum16 index)
{
	if(probe->capture_done) return;
	if(!probe->capture_frames)
	{
		if(!probe_has_nal(frame, size, 5)) return;
		memcpy(probe->capture_data, probe->original_video_header, probe->original_video_header_size);
		probe->capture_size = probe->original_video_header_size;
		probe->capture_started = sceKernelGetProcessTimeWide() / 1000u;
		CHIAKI_LOGI(probe->log, "Stream probe H264 capture started at IDR frame=%u; SPS/PPS unmodified", (unsigned int)index);
	}
	else if(index != (ChiakiSeqNum16)(probe->capture_last_frame + 1))
	{
		probe->capture_gap = probe->capture_done = true;
		CHIAKI_LOGE(probe->log, "Stream probe H264 capture gap: previous=%u current=%u", (unsigned int)probe->capture_last_frame, (unsigned int)index);
		return;
	}
	if(size > PROBE_CAPTURE_BYTES_MAX - probe->capture_size)
	{
		probe->capture_done = true;
		return;
	}
	memcpy(probe->capture_data + probe->capture_size, frame, size);
	probe->capture_size += size;
	probe->capture_frames++;
	probe->capture_last_frame = index;
	if(probe->capture_frames >= 300
		|| sceKernelGetProcessTimeWide() / 1000u - probe->capture_started >= PROBE_CAPTURE_MS)
		probe->capture_done = true;
	chiaki_cond_signal(&probe->cond);
}

static ChiakiErrorCode probe_video_start_frame(PlusVitaStreamProbe *probe,
	ProbeVideoAssembly *assembly, ChiakiTakionAVPacket *packet)
{
	if(packet->units_in_frame_total < packet->units_in_frame_fec
		|| packet->units_in_frame_total > PROBE_VIDEO_UNITS_MAX)
		return CHIAKI_ERR_INVALID_DATA;
	probe_video_assembly_reset(assembly);
	assembly->active = true;
	assembly->frame_index = packet->frame_index;
	assembly->units_total = packet->units_in_frame_total;
	assembly->units_fec = packet->units_in_frame_fec;
	assembly->units_source = packet->units_in_frame_total
		- packet->units_in_frame_fec;
	if(!assembly->units_source)
		return CHIAKI_ERR_INVALID_DATA;
	if(assembly->units_fec)
		probe->fec_frames_advertised++;
	return CHIAKI_ERR_SUCCESS;
}

static void probe_reception_flush(PlusVitaStreamProbe *probe, bool pressure)
{
	for(;;)
	{
		/* Never mark these slots ready: congestion accounting keeps actual
		 * source AND parity receipts until the same bounded late-packet grace
		 * expires, even if the decoder has already received a complete AU. */
		PlusVitaVideoReorderEvent event = plus_vita_video_reorder_poll(
			&probe->feedback_window, sceKernelGetProcessTimeWide(), pressure);
		if(event.action == PLUS_VITA_VIDEO_REORDER_WAIT) return;
		if(event.slot < 0) continue; /* unknown geometry: do not invent loss */
		ProbeReceptionFrame *frame = &probe->feedback_frames[event.slot];
		probe->feedback_pending_received += frame->received;
		probe->feedback_pending_lost += frame->expected - frame->received;
		memset(frame, 0, sizeof(*frame));
		pressure = false; /* one free slot is enough; retain the other frames */
	}
}

static ChiakiErrorCode probe_record_reception(PlusVitaStreamProbe *probe,
	const ChiakiTakionAVPacket *packet)
{
	if(!packet->units_in_frame_total
		|| packet->units_in_frame_total > PROBE_VIDEO_UNITS_MAX
		|| packet->units_in_frame_fec > packet->units_in_frame_total
		|| packet->unit_index >= packet->units_in_frame_total)
		return CHIAKI_ERR_INVALID_DATA;
	probe_reception_flush(probe, false);
	int slot = plus_vita_video_reorder_admit(&probe->feedback_window,
		packet->frame_index, sceKernelGetProcessTimeWide());
	if(slot == PLUS_VITA_VIDEO_REORDER_FULL)
	{
		probe_reception_flush(probe, true);
		slot = plus_vita_video_reorder_admit(&probe->feedback_window,
			packet->frame_index, sceKernelGetProcessTimeWide());
	}
	if(slot == PLUS_VITA_VIDEO_REORDER_STALE) return CHIAKI_ERR_SUCCESS;
	if(slot < 0) return CHIAKI_ERR_OVERFLOW;
	ProbeReceptionFrame *frame = &probe->feedback_frames[slot];
	if(frame->expected && frame->profile != packet->adaptive_stream_index)
	{
		probe->feedback_pending_received += frame->received;
		probe->feedback_pending_lost += frame->expected - frame->received;
		memset(frame, 0, sizeof(*frame));
	}
	if(!frame->expected)
	{
		frame->expected = packet->units_in_frame_total;
		frame->profile = packet->adaptive_stream_index;
	}
	if(packet->units_in_frame_total != frame->expected)
		return CHIAKI_ERR_INVALID_DATA;
	if(!frame->units[packet->unit_index])
	{
		frame->units[packet->unit_index] = true;
		frame->received++;
	}
	return CHIAKI_ERR_SUCCESS;
}

static ChiakiErrorCode probe_send_protobuf(PlusVitaStreamProbe *probe,
	tkproto_TakionMessage *msg, uint8_t channel);

static bool probe_wire_next(ChiakiSeqNum16 previous, ChiakiSeqNum16 current)
{
	return current == (ChiakiSeqNum16)(previous + 1);
}

static ChiakiErrorCode probe_video_submit(PlusVitaStreamProbe *probe,
	ProbeVideoAssembly *assembly);

/* Caller holds the same mutex as the receive callback. A timer in the main
 * loop also drains this window, so a missing packet cannot wait indefinitely.
 * Retirement never advances decoder/reference state: submit still detects
 * any real wire gap and requests a validated IDR. */
static ChiakiErrorCode probe_video_drain(PlusVitaStreamProbe *probe, bool pressure)
{
	for(;;)
	{
		PlusVitaVideoReorderEvent event = plus_vita_video_reorder_poll(
			&probe->video_reorder, sceKernelGetProcessTimeWide(), pressure);
		if(event.action == PLUS_VITA_VIDEO_REORDER_WAIT) return CHIAKI_ERR_SUCCESS;
		if(event.wait_us)
		{
			probe->reorder_waits++;
			probe->reorder_wait_total_us += event.wait_us;
			if(event.wait_us > probe->reorder_wait_max_us)
				probe->reorder_wait_max_us = event.wait_us;
		}
		if(event.action == PLUS_VITA_VIDEO_REORDER_DROP)
		{
			if(event.slot >= 0)
			{
				probe->incomplete_frames++;
				probe->reorder_expired_frames++;
				probe_video_assembly_reset(&probe->video_assembly[event.slot]);
				pressure = false;
			}
			else probe->reorder_missing_frames += event.skipped;
			if(event.pressure) probe->reorder_pressure_drops++;
			if(probe->capture_frames && !probe->capture_done)
				probe->capture_gap = probe->capture_done = true;
			CHIAKI_LOGW(probe->log,
				"Stream probe reorder expired wire_frame=%u skipped=%u partial=%d pressure=%d wait_us=%llu",
				(unsigned int)event.index, (unsigned int)event.skipped,
				event.slot >= 0 ? 1 : 0, event.pressure ? 1 : 0,
				(unsigned long long)event.wait_us);
			continue;
		}
		ProbeVideoAssembly *assembly = &probe->video_assembly[event.slot];
		if(assembly->late_received)
		{
			probe->reorder_saved_frames++;
			CHIAKI_LOGI(probe->log, "Stream probe reorder saved wire_frame=%u wait_us=%llu",
				(unsigned int)event.index, (unsigned long long)event.wait_us);
		}
		probe->reorder_held_total_us += event.held_us;
		if(event.held_us > probe->reorder_held_max_us)
			probe->reorder_held_max_us = event.held_us;
		ChiakiErrorCode err = probe_video_submit(probe, assembly);
		if(err != CHIAKI_ERR_SUCCESS) return err;
		pressure = false;
	}
}

static ChiakiErrorCode probe_video_packet(PlusVitaStreamProbe *probe,
	ChiakiTakionAVPacket *packet)
{
	unsigned int index = packet->adaptive_stream_index;
	if(index >= 8)
		return CHIAKI_ERR_INVALID_DATA;
	ChiakiErrorCode reception_err = probe_record_reception(probe, packet);
	if(reception_err != CHIAKI_ERR_SUCCESS)
		return reception_err;
	probe->profile_packet_counts[index]++;
	if(!probe->last_profile_valid || probe->last_profile_index != index)
	{
		CHIAKI_LOGI(probe->log,
			"Stream probe server video profile index=%u packet=%u",
			index, probe->video_packets);
		probe->last_profile_index = index;
		probe->last_profile_valid = true;
	}
	/* Do not feed another profile's access units into a decoder using
	 * the selected candidate's SPS/PPS. */
	if(index != probe->selected_profile_index)
	{
		if(!PROBE_CAPTURE_ONLY && probe->reference_guard.started)
		{
			CHIAKI_LOGE(probe->log, "Stream probe LIVE_GUARD_REJECT: profile switch after IDR");
			return CHIAKI_ERR_INVALID_DATA;
		}
		if(probe->capture_frames && !probe->capture_done) probe->capture_gap = probe->capture_done = true;
		for(unsigned int i = 0; i < PLUS_VITA_VIDEO_REORDER_SLOTS; i++)
			probe_video_assembly_reset(&probe->video_assembly[i]);
		memset(&probe->video_reorder, 0, sizeof(probe->video_reorder));
		return CHIAKI_ERR_SUCCESS;
	}
	if(packet->unit_index >= packet->units_in_frame_total
		- packet->units_in_frame_fec)
		probe->fec_packets_seen++;
	ChiakiErrorCode drain_err = probe_video_drain(probe, false);
	if(drain_err != CHIAKI_ERR_SUCCESS) return drain_err;
	int slot = plus_vita_video_reorder_admit(&probe->video_reorder,
		packet->frame_index, sceKernelGetProcessTimeWide());
	if(slot == PLUS_VITA_VIDEO_REORDER_FULL)
	{
		drain_err = probe_video_drain(probe, true);
		if(drain_err != CHIAKI_ERR_SUCCESS) return drain_err;
		slot = plus_vita_video_reorder_admit(&probe->video_reorder,
			packet->frame_index, sceKernelGetProcessTimeWide());
	}
	if(slot == PLUS_VITA_VIDEO_REORDER_STALE)
	{
		probe->reorder_retired_packets++;
		/* Parity arriving after an already completed frame is normal and was
		 * not included in the old stale counter. Keep that comparison useful. */
		if(!probe->video_frame_completed_valid
			|| chiaki_seq_num_16_gt(packet->frame_index, probe->video_frame_completed))
			probe->stale_packets++;
		return CHIAKI_ERR_SUCCESS;
	}
	if(slot < 0) return CHIAKI_ERR_OVERFLOW;
	ProbeVideoAssembly *assembly = &probe->video_assembly[slot];
	if(!assembly->active)
	{
		ChiakiErrorCode err = probe_video_start_frame(probe, assembly, packet);
		if(err != CHIAKI_ERR_SUCCESS)
			return err;
		assembly->first_packet_us = probe->callback_arrival_us;
		unsigned int occupied = 0;
		for(unsigned int i = 0; i < PLUS_VITA_VIDEO_REORDER_SLOTS; i++)
			if(probe->video_reorder.slots[i].used) occupied++;
		if(occupied > probe->reorder_peak) probe->reorder_peak = occupied;
	}
	if(packet->units_in_frame_total != assembly->units_total
		|| packet->units_in_frame_fec != assembly->units_fec
		|| packet->unit_index >= assembly->units_total)
		return CHIAKI_ERR_INVALID_DATA;
	if(packet->data_size < 2)
		return CHIAKI_ERR_BUF_TOO_SMALL;
	if(assembly->unit_data[packet->unit_index])
		return CHIAKI_ERR_SUCCESS;
	/* Three slots must remain a fixed memory budget even for malformed wire
	 * geometry. Includes parity and packet prefixes, unlike the AU size cap. */
	if(packet->data_size > 65536u
		|| packet->data_size > 2u * PROBE_FRAME_BYTES_MAX - assembly->stored_bytes)
		return CHIAKI_ERR_OVERFLOW;
	if(probe->video_reorder.slots[slot].ready) return CHIAKI_ERR_SUCCESS;
	for(unsigned int i = 0; i < PLUS_VITA_VIDEO_REORDER_SLOTS; i++)
	{
		if(probe->video_reorder.slots[i].used
			&& chiaki_seq_num_16_gt(probe->video_reorder.slots[i].index, packet->frame_index))
		{
			assembly->late_received = true;
			probe->reorder_late_packets++;
			break;
		}
	}
	uint8_t *unit = malloc(packet->data_size);
	if(!unit)
		return CHIAKI_ERR_MEMORY;
	memcpy(unit, packet->data, packet->data_size);
	assembly->unit_data[packet->unit_index] = unit;
	assembly->unit_size[packet->unit_index] = packet->data_size;
	assembly->stored_bytes += packet->data_size;
	if(packet->unit_index < assembly->units_source)
		assembly->units_received++;
	else
		assembly->parity_received++;
	if(assembly->units_received < assembly->units_source
		&& assembly->units_received + assembly->parity_received >= assembly->units_source
		&& !assembly->fec_attempted)
	{
		assembly->fec_attempted = true;
		probe->fec_attempts++;
		unsigned repaired = 0;
		uint64_t fec_started = sceKernelGetProcessTimeWide();
		ChiakiErrorCode repair_err = plus_vita_video_fec_repair(
			assembly->unit_data, assembly->unit_size, assembly->units_source,
			assembly->units_fec, &repaired);
		uint64_t fec_us = sceKernelGetProcessTimeWide() - fec_started;
		probe->fec_total_us += fec_us;
		if(fec_us > probe->fec_max_us) probe->fec_max_us = fec_us;
		if(repair_err == CHIAKI_ERR_SUCCESS)
		{
			assembly->stored_bytes = 0;
			for(unsigned int i = 0; i < assembly->units_total; i++)
			{
				if(assembly->unit_size[i] > 2u * PROBE_FRAME_BYTES_MAX - assembly->stored_bytes)
					return CHIAKI_ERR_OVERFLOW;
				assembly->stored_bytes += assembly->unit_size[i];
			}
			assembly->units_received += repaired;
			probe->fec_repaired_frames++;
			probe->fec_repaired_units += repaired;
			CHIAKI_LOGI(probe->log,
				"Stream probe FEC repaired wire_frame=%u units=%u source=%u parity=%u repair_us=%llu",
				(unsigned int)assembly->frame_index, repaired,
				(unsigned int)assembly->units_source, (unsigned int)assembly->units_fec,
				(unsigned long long)fec_us);
		}
		else
		{
			probe->fec_failed++;
			CHIAKI_LOGW(probe->log,
				"Stream probe FEC repair failed wire_frame=%u error=%d; waiting for source units or validated-IDR recovery",
				(unsigned int)assembly->frame_index, (int)repair_err);
			if(repair_err == CHIAKI_ERR_MEMORY) return repair_err;
		}
	}
	if(assembly->units_received < assembly->units_source)
		return probe_video_drain(probe, false);
	assembly->ready_us = sceKernelGetProcessTimeWide();
	if(probe->latency_collecting)
		plus_vita_latency_span(&probe->latency.frame_assembly,
			assembly->first_packet_us, assembly->ready_us);
	plus_vita_video_reorder_ready(&probe->video_reorder, slot, assembly->ready_us);
	return probe_video_drain(probe, false);
}

static ChiakiErrorCode probe_video_submit(PlusVitaStreamProbe *probe,
	ProbeVideoAssembly *assembly)
{
	size_t frame_size = 0;
	for(unsigned int i = 0; i < assembly->units_source; i++)
	{
		if(!assembly->unit_data[i] || assembly->unit_size[i] < 2)
			return CHIAKI_ERR_INVALID_DATA;
		if(frame_size > SIZE_MAX - (assembly->unit_size[i] - 2))
			return CHIAKI_ERR_OVERFLOW;
		frame_size += assembly->unit_size[i] - 2;
	}
	if(!frame_size || frame_size > PROBE_FRAME_BYTES_MAX)
		return CHIAKI_ERR_OVERFLOW;
	uint8_t *frame = malloc(frame_size + CHIAKI_VIDEO_BUFFER_PADDING_SIZE);
	if(!frame)
		return CHIAKI_ERR_MEMORY;
	size_t pos = 0;
	for(unsigned int i = 0; i < assembly->units_source; i++)
	{
		size_t part_size = assembly->unit_size[i] - 2;
		memcpy(frame + pos, assembly->unit_data[i] + 2, part_size);
		pos += part_size;
	}
	memset(frame + frame_size, 0, CHIAKI_VIDEO_BUFFER_PADDING_SIZE);
	/* Controlled local test: discard exactly one post-IDR P AU, without
	 * advancing wire/reference state. The following AU must trigger recovery. */
	if(PROBE_RECOVERY_FAULT_TEST && !PROBE_CAPTURE_ONLY
		&& !probe->recovery_fault_injected && probe->video_frames >= 120
		&& probe->reference_guard.started && !probe->awaiting_recovery_idr
		&& probe_has_nal(frame, frame_size, 1) && !probe_has_nal(frame, frame_size, 5))
	{
		probe->recovery_fault_injected = true;
		probe->capture_gap = probe->capture_done = true;
		CHIAKI_LOGW(probe->log, "Stream probe recovery TEST_DROP wire_frame=%u bytes=%lu; one deliberate local AU omission",
			(unsigned int)assembly->frame_index, (unsigned long)frame_size);
		free(frame);
		probe_video_assembly_reset(assembly);
		return CHIAKI_ERR_SUCCESS;
	}
	if(PROBE_CAPTURE_ONLY)
		probe_capture_frame(probe, frame, frame_size, assembly->frame_index);
	if(!PROBE_CAPTURE_ONLY)
	{
		if(!probe->reference_guard.started && !probe_has_nal(frame, frame_size, 5))
		{
			free(frame);
			probe_video_assembly_reset(assembly);
			return CHIAKI_ERR_SUCCESS;
		}
		/* H.264 frame_num resets at IDR. Use the independent transport
		 * generation to distinguish a new reset from a duplicate/gap. */
		if(!probe->awaiting_recovery_idr && probe->reference_guard.started && (!probe->video_frame_completed_valid
			|| !probe_wire_next(probe->video_frame_completed, assembly->frame_index)))
		{
			CHIAKI_LOGW(probe->log, "Stream probe recovery WIRE_FRAME_GAP previous=%u observed=%u; waiting for validated IDR",
				(unsigned int)probe->video_frame_completed, (unsigned int)assembly->frame_index);
			probe->awaiting_recovery_idr = true;
			probe->recovery_started_ms = sceKernelGetProcessTimeWide() / 1000u;
			probe->recovery_count++;
			probe->recovery_report_start = (ChiakiSeqNum16)(probe->video_frame_completed + 1);
			probe->recovery_last_request_ms = probe->recovery_started_ms;
			probe->recovery_requests++;
			tkproto_TakionMessage msg = { 0 };
			msg.type = tkproto_TakionMessage_PayloadType_CORRUPTFRAME;
			msg.has_corrupt_payload = true;
			msg.corrupt_payload.start = (ChiakiSeqNum16)(probe->video_frame_completed + 1);
			msg.corrupt_payload.end = (ChiakiSeqNum16)(assembly->frame_index - 1);
			ChiakiErrorCode request = probe_send_protobuf(probe, &msg, 2);
			CHIAKI_LOGI(probe->log, "Stream probe recovery CORRUPTFRAME start=%u end=%u send=%d",
				(unsigned int)msg.corrupt_payload.start, (unsigned int)msg.corrupt_payload.end, (int)request);
			if(request != CHIAKI_ERR_SUCCESS) { free(frame); return request; }
		}
		if(probe->awaiting_recovery_idr && !probe_has_nal(frame, frame_size, 5))
		{
			/* The reference chain is broken. Never submit dependent P frames. */
			uint64_t recovery_now = sceKernelGetProcessTimeWide() / 1000u;
			/* Re-report the unusable chain, not only the first missing AU.
			 * Rate-limited to ten reports per five-second recovery episode. */
			if(recovery_now - probe->recovery_last_request_ms >= 500
				&& recovery_now - probe->recovery_started_ms < 5000)
			{
				tkproto_TakionMessage retry = { 0 };
				retry.type = tkproto_TakionMessage_PayloadType_CORRUPTFRAME;
				retry.has_corrupt_payload = true;
				retry.corrupt_payload.start = probe->recovery_report_start;
				retry.corrupt_payload.end = assembly->frame_index;
				ChiakiErrorCode request = probe_send_protobuf(probe, &retry, 2);
				probe->recovery_last_request_ms = recovery_now;
				probe->recovery_requests++;
				CHIAKI_LOGI(probe->log, "Stream probe recovery CORRUPTFRAME retry start=%u end=%u send=%d requests=%u",
					(unsigned int)retry.corrupt_payload.start, (unsigned int)retry.corrupt_payload.end,
					(int)request, probe->recovery_requests);
				if(request != CHIAKI_ERR_SUCCESS) { free(frame); return request; }
			}
			probe->recovery_dropped++;
			probe->video_frame_completed = assembly->frame_index;
			probe->video_frame_completed_valid = true;
			free(frame);
			probe_video_assembly_reset(assembly);
			return CHIAKI_ERR_SUCCESS;
		}
		uint32_t idr_before = probe->reference_guard.idr_count;
		if(!plus_vita_h264_guard_frame(&probe->reference_guard, frame, frame_size))
		{
			CHIAKI_LOGE(probe->log,
				"Stream probe LIVE_GUARD_REJECT reason=%s wire_frame=%u previous_frame_num=%u observed_frame_num=%u nal=%u slice=%u mb=%u bytes=%lu incomplete=%u; AU not submitted",
				probe->reference_guard.reason, (unsigned int)assembly->frame_index,
				probe->reference_guard.frame_num, probe->reference_guard.observed_frame,
				probe->reference_guard.observed_nal, probe->reference_guard.observed_slice,
				probe->reference_guard.observed_mb, (unsigned long)frame_size, probe->incomplete_frames);
			free(frame);
			return CHIAKI_ERR_INVALID_DATA;
		}
		if(probe->reference_guard.idr_count != idr_before)
			CHIAKI_LOGI(probe->log, "Stream probe validated IDR reset count=%u wire_frame=%u frame_num=0 long_term=0",
				probe->reference_guard.idr_count, (unsigned int)assembly->frame_index);
		if(probe->awaiting_recovery_idr)
		{
			CHIAKI_LOGI(probe->log, "Stream probe recovery resumed at validated IDR wire_frame=%u wait_ms=%llu dropped=%u",
				(unsigned int)assembly->frame_index,
				(unsigned long long)(sceKernelGetProcessTimeWide() / 1000u - probe->recovery_started_ms), probe->recovery_dropped);
			probe->awaiting_recovery_idr = false;
			probe->recovery_resumes++;
			probe->recovery_resumed_ms = sceKernelGetProcessTimeWide() / 1000u;
			probe->recovery_output_pending = true;
		}
	}
	ChiakiBitstreamSlice slice = { 0 };
	bool parsed = probe->bitstream_ready && chiaki_bitstream_slice(
		&probe->bitstream, frame, (unsigned int)frame_size, &slice);
	if(!PROBE_CAPTURE_ONLY && !parsed)
	{
		CHIAKI_LOGE(probe->log, "Stream probe LIVE_GUARD_REJECT: transport slice parser disagrees; AU not submitted");
		free(frame);
		return CHIAKI_ERR_INVALID_DATA;
	}
	probe->video_frames++;
	if(parsed && slice.slice_type == CHIAKI_BITSTREAM_SLICE_I)
		probe->video_keyframe = true;
	if(probe->video_frames <= 4 || probe->video_frames % 60 == 0)
		CHIAKI_LOGI(probe->log,
		"Stream probe complete video frame: size=%lu parsed=%d type=%d keyframe=%d",
		(unsigned long)frame_size, parsed ? 1 : 0, (int)slice.slice_type,
		probe->video_keyframe ? 1 : 0);
	/* Transfer ownership to the main thread. Never decode, scale, or wait
	 * for display refresh inside Takion's receive callback. */
	if(parsed && probe->video_keyframe && probe->video_decoder_ready)
	{
		if(probe->queue_count == PROBE_QUEUE_MAX)
		{
			CHIAKI_LOGE(probe->log, "Stream probe preview queue full; stopping instead of discarding reference frames");
			free(frame);
			return CHIAKI_ERR_OVERFLOW;
		}
		unsigned int slot = (probe->queue_head + probe->queue_count) % PROBE_QUEUE_MAX;
		probe->queue[slot] = (ProbeQueuedFrame){ .data = frame, .size = frame_size,
			.include_header = slice.slice_type == CHIAKI_BITSTREAM_SLICE_I,
			.recovery_generation = slice.slice_type == CHIAKI_BITSTREAM_SLICE_I ? probe->recovery_resumes : 0,
			.enqueued_us = sceKernelGetProcessTimeWide(),
			.first_packet_us = assembly->first_packet_us, .ready_us = assembly->ready_us };
		if(probe->latency_collecting)
			plus_vita_latency_span(&probe->latency.ready_to_enqueue,
				assembly->ready_us, probe->queue[slot].enqueued_us);
		probe->queue_count++;
		if(probe->queue_count > probe->queue_peak)
			probe->queue_peak = probe->queue_count;
		frame = NULL;
		chiaki_cond_signal(&probe->cond);
	}
	free(frame);
	probe->video_frame_completed = assembly->frame_index;
	probe->video_frame_completed_valid = true;
	probe_video_assembly_reset(assembly);
	return CHIAKI_ERR_SUCCESS;
}

static ChiakiErrorCode probe_audio_packet(PlusVitaStreamProbe *probe,
	ChiakiTakionAVPacket *packet)
{
	if(packet->codec != 5 || !packet->data_size)
		return CHIAKI_ERR_INVALID_DATA;
	uint8_t source_units =
		chiaki_takion_av_packet_audio_source_units_count(packet);
	uint8_t fec_units = chiaki_takion_av_packet_audio_fec_units_count(packet);
	uint8_t unit_size = chiaki_takion_av_packet_audio_unit_size(packet);
	if(!source_units || !unit_size
		|| (uint16_t)source_units + (uint16_t)fec_units
			!= packet->units_in_frame_total
		|| packet->data_size != (size_t)unit_size
			* packet->units_in_frame_total)
	{
		CHIAKI_LOGE(probe->log,
			"Stream probe invalid audio layout: size=%lu unit=%u source=%u fec=%u total=%u packed=%u",
			(unsigned long)packet->data_size, (unsigned int)unit_size,
			(unsigned int)source_units, (unsigned int)fec_units,
			(unsigned int)packet->units_in_frame_total,
			(unsigned int)packet->units_in_frame_fec);
		return CHIAKI_ERR_INVALID_DATA;
	}
	bool first_audio_frame = probe->audio_frames == 0;
	for(unsigned int i = 0; i < source_units; i++)
		if(!plus_vita_audio_push(probe->audio_playback, (uint16_t)(packet->frame_index + i),
			packet->data + i * unit_size, unit_size)) return CHIAKI_ERR_INVALID_DATA;
	probe->audio_frames += source_units;
	if(first_audio_frame)
		CHIAKI_LOGI(probe->log,
			"Stream probe complete Opus frame: size=%u frame=%u source=%u fec=%u",
			(unsigned int)unit_size, (unsigned int)packet->frame_index,
			(unsigned int)source_units, (unsigned int)fec_units);
	return CHIAKI_ERR_SUCCESS;
}

static ChiakiErrorCode probe_send_protobuf(PlusVitaStreamProbe *probe,
	tkproto_TakionMessage *msg, uint8_t channel)
{
	uint8_t buf[2048];
	pb_ostream_t stream = pb_ostream_from_buffer(buf, sizeof(buf));
	if(!pb_encode(&stream, tkproto_TakionMessage_fields, msg))
		return CHIAKI_ERR_INVALID_DATA;
	return chiaki_takion_send_message_data(&probe->takion, 1, channel,
		buf, stream.bytes_written, NULL);
}

static ChiakiErrorCode probe_send_streaminfo_ack(PlusVitaStreamProbe *probe)
{
	tkproto_TakionMessage msg = { 0 };
	msg.type = tkproto_TakionMessage_PayloadType_STREAMINFOACK;
	return probe_send_protobuf(probe, &msg, 9);
}

static ChiakiErrorCode probe_send_controller(PlusVitaStreamProbe *probe)
{
	tkproto_TakionMessage msg = { 0 };
	msg.type = tkproto_TakionMessage_PayloadType_CONTROLLERCONNECTION;
	msg.has_controller_connection_payload = true;
	msg.controller_connection_payload.has_connected = true;
	msg.controller_connection_payload.connected = true;
	msg.controller_connection_payload.has_controller_type = true;
	msg.controller_connection_payload.controller_type =
		tkproto_ControllerConnectionPayload_ControllerType_DUALSHOCK4;
	return probe_send_protobuf(probe, &msg, 1);
}

static ChiakiErrorCode probe_enable_microphone(PlusVitaStreamProbe *probe)
{
	ChiakiAudioHeader input;
	chiaki_audio_header_set(&input, 16, 1, 48000, 480);
	uint8_t audio_header[CHIAKI_AUDIO_HEADER_SIZE];
	chiaki_audio_header_save(&input, audio_header);
	ChiakiPBBuf audio_header_buf = {
		.size = sizeof(audio_header), .buf = audio_header };
	tkproto_TakionMessage msg = { 0 };
	msg.type = tkproto_TakionMessage_PayloadType_STREAMINFO;
	msg.has_stream_info_payload = true;
	msg.stream_info_payload.audio_header.arg = &audio_header_buf;
	msg.stream_info_payload.audio_header.funcs.encode = chiaki_pb_encode_buf;
	return probe_send_protobuf(probe, &msg, 1);
}

static ChiakiErrorCode probe_send_disconnect(PlusVitaStreamProbe *probe,
	const char *reason)
{
	tkproto_TakionMessage msg = { 0 };
	msg.type = tkproto_TakionMessage_PayloadType_DISCONNECT;
	msg.has_disconnect_payload = true;
	msg.disconnect_payload.reason.arg = (void *)reason;
	msg.disconnect_payload.reason.funcs.encode = chiaki_pb_encode_string;
	return probe_send_protobuf(probe, &msg, 1);
}

static ChiakiErrorCode probe_derive_secret(PlusVitaStreamProbe *probe,
	uint8_t *secret_out, const uint8_t *remote_key, size_t remote_key_size)
{
	mbedtls_ecp_point peer;
	mbedtls_mpi shared;
	mbedtls_ecp_point_init(&peer);
	mbedtls_mpi_init(&shared);
	mbedtls_ecp_group *group = &probe->ecdh.ctx.ctx.mbed_ecdh.grp;
	mbedtls_mpi *private_key = &probe->ecdh.ctx.ctx.mbed_ecdh.d;

	int native_result = mbedtls_ecp_point_read_binary(group, &peer,
		remote_key, remote_key_size);
	CHIAKI_LOGI(probe->log,
		"Stream probe ECP point parse result=%d", native_result);
	if(native_result != 0)
		goto error;

	native_result = mbedtls_ecp_check_pubkey(group, &peer);
	CHIAKI_LOGI(probe->log,
		"Stream probe ECP point validation result=%d", native_result);
	if(native_result != 0)
		goto error;

	native_result = mbedtls_ecdh_compute_shared(group, &shared, &peer,
		private_key, mbedtls_ctr_drbg_random,
		&probe->ecdh.drbg);
	CHIAKI_LOGI(probe->log,
		"Stream probe ECP shared-secret result=%d", native_result);
	if(native_result != 0)
		goto error;

	native_result = mbedtls_mpi_write_binary(&shared, secret_out,
		CHIAKI_ECDH_SECRET_SIZE);
	CHIAKI_LOGI(probe->log,
		"Stream probe ECP secret export result=%d size=%u", native_result,
		(unsigned int)CHIAKI_ECDH_SECRET_SIZE);
	mbedtls_mpi_free(&shared);
	mbedtls_ecp_point_free(&peer);
	if(native_result != 0)
		return CHIAKI_ERR_UNKNOWN;
	return CHIAKI_ERR_SUCCESS;

error:
	mbedtls_mpi_free(&shared);
	mbedtls_ecp_point_free(&peer);
	return CHIAKI_ERR_UNKNOWN;
}

static void probe_log_disconnect(PlusVitaStreamProbe *probe, uint8_t *buf,
	size_t buf_size)
{
	char reason[256];
	ChiakiPBDecodeBuf reason_buf = {
		.max_size = sizeof(reason) - 1, .size = 0, .buf = (uint8_t *)reason };
	tkproto_TakionMessage msg = { 0 };
	msg.disconnect_payload.reason.arg = &reason_buf;
	msg.disconnect_payload.reason.funcs.decode = chiaki_pb_decode_buf;
	pb_istream_t stream = pb_istream_from_buffer(buf, buf_size);
	if(!pb_decode(&stream, tkproto_TakionMessage_fields, &msg))
	{
		CHIAKI_LOGE(probe->log,
			"Stream probe could not decode remote disconnect (%lu bytes)",
			(unsigned long)buf_size);
		return;
	}
	reason[reason_buf.size] = '\0';
	CHIAKI_LOGE(probe->log, "Stream probe remote disconnect: %s",
		reason_buf.size ? reason : "(no reason supplied)");
}

static void probe_handle_bang(PlusVitaStreamProbe *probe, uint8_t *buf,
	size_t buf_size)
{
	uint8_t remote_key[128];
	ChiakiPBDecodeBuf remote_key_buf = {
		.max_size = sizeof(remote_key), .size = 0, .buf = remote_key };
	uint8_t remote_sig[32];
	ChiakiPBDecodeBuf remote_sig_buf = {
		.max_size = sizeof(remote_sig), .size = 0, .buf = remote_sig };
	tkproto_TakionMessage msg = { 0 };
	msg.bang_payload.ecdh_pub_key.arg = &remote_key_buf;
	msg.bang_payload.ecdh_pub_key.funcs.decode = chiaki_pb_decode_buf;
	msg.bang_payload.ecdh_sig.arg = &remote_sig_buf;
	msg.bang_payload.ecdh_sig.funcs.decode = chiaki_pb_decode_buf;
	pb_istream_t stream = pb_istream_from_buffer(buf, buf_size);
	if(!pb_decode(&stream, tkproto_TakionMessage_fields, &msg))
	{
		CHIAKI_LOGE(probe->log,
			"Stream probe could not decode server protobuf (%lu bytes)",
			(unsigned long)buf_size);
		probe->state = PROBE_FAILED;
		chiaki_cond_signal(&probe->cond);
		return;
	}
	if(msg.type == tkproto_TakionMessage_PayloadType_DISCONNECT)
	{
		probe_log_disconnect(probe, buf, buf_size);
		probe->state = PROBE_FAILED;
		chiaki_cond_signal(&probe->cond);
		return;
	}
	if(msg.type != tkproto_TakionMessage_PayloadType_BANG
		|| !msg.has_bang_payload)
	{
		CHIAKI_LOGE(probe->log,
			"Stream probe expected BANG but received protobuf type %d",
			(int)msg.type);
		probe->state = PROBE_FAILED;
		chiaki_cond_signal(&probe->cond);
		return;
	}
	if(!msg.bang_payload.version_accepted
		|| !msg.bang_payload.encrypted_key_accepted
		|| !remote_key_buf.size || !remote_sig_buf.size)
	{
		CHIAKI_LOGE(probe->log, "Stream probe BANG rejected or incomplete");
		probe->state = PROBE_FAILED;
		chiaki_cond_signal(&probe->cond);
		return;
	}
	CHIAKI_LOGI(probe->log,
		"Stream probe BANG fields accepted: public_key=%lu signature=%lu",
		(unsigned long)remote_key_buf.size, (unsigned long)remote_sig_buf.size);
	uint8_t expected_sig[32];
	const mbedtls_md_info_t *sha256 = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
	if(remote_sig_buf.size != sizeof(expected_sig) || !sha256
		|| mbedtls_md_hmac(sha256, probe->handshake_key,
			sizeof(probe->handshake_key), remote_key, remote_key_buf.size,
			expected_sig) != 0)
	{
		CHIAKI_LOGE(probe->log, "Stream probe could not verify BANG signature");
		probe->state = PROBE_FAILED;
		chiaki_cond_signal(&probe->cond);
		return;
	}
	unsigned int sig_difference = 0;
	for(size_t i = 0; i < sizeof(expected_sig); i++)
		sig_difference |= expected_sig[i] ^ remote_sig[i];
	memset(expected_sig, 0, sizeof(expected_sig));
	if(sig_difference)
	{
		CHIAKI_LOGE(probe->log, "Stream probe rejected invalid BANG signature");
		probe->state = PROBE_FAILED;
		chiaki_cond_signal(&probe->cond);
		return;
	}
	uint8_t secret[CHIAKI_ECDH_SECRET_SIZE];
	ChiakiErrorCode err = probe_derive_secret(probe, secret, remote_key,
		remote_key_buf.size);
	CHIAKI_LOGI(probe->log, "Stream probe ECDH derive result=%d", (int)err);
	if(err == CHIAKI_ERR_SUCCESS)
	{
		probe->crypt_local = chiaki_gkcrypt_new(probe->log, 0, 2,
			probe->handshake_key, secret);
		CHIAKI_LOGI(probe->log, "Stream probe local GKCrypt result=%s",
			probe->crypt_local ? "ready" : "failed");
		probe->crypt_remote = chiaki_gkcrypt_new(probe->log, 0, 3,
			probe->handshake_key, secret);
		CHIAKI_LOGI(probe->log, "Stream probe remote GKCrypt result=%s",
			probe->crypt_remote ? "ready" : "failed");
	}
	memset(secret, 0, sizeof(secret));
	if(!probe->crypt_local || !probe->crypt_remote)
	{
		CHIAKI_LOGE(probe->log, "Stream probe failed to derive transport crypto");
		probe->state = PROBE_FAILED;
	}
	else
	{
		chiaki_takion_set_crypt(&probe->takion, probe->crypt_local,
			probe->crypt_remote);
		CHIAKI_LOGI(probe->log,
			"Stream probe BANG accepted; waiting for STREAMINFO");
		probe->state = PROBE_EXPECT_STREAMINFO;
	}
	chiaki_cond_signal(&probe->cond);
}

static void probe_handle_streaminfo(PlusVitaStreamProbe *probe, uint8_t *buf,
	size_t buf_size)
{
	uint8_t audio_header[CHIAKI_AUDIO_HEADER_SIZE];
	ChiakiPBDecodeBuf audio_header_buf = {
		.max_size = sizeof(audio_header), .size = 0, .buf = audio_header };
	ProbeStreamInfoContext context = {
		.log = probe->log, .profiles_valid = true };
	tkproto_TakionMessage msg = { 0 };
	msg.stream_info_payload.audio_header.arg = &audio_header_buf;
	msg.stream_info_payload.audio_header.funcs.decode = chiaki_pb_decode_buf;
	msg.stream_info_payload.resolution.arg = &context;
	msg.stream_info_payload.resolution.funcs.decode = probe_decode_resolution;
	pb_istream_t stream = pb_istream_from_buffer(buf, buf_size);
	if(!pb_decode(&stream, tkproto_TakionMessage_fields, &msg))
	{
		CHIAKI_LOGE(probe->log,
			"Stream probe could not decode STREAMINFO (%lu bytes)",
			(unsigned long)buf_size);
		probe->state = PROBE_FAILED;
		chiaki_cond_signal(&probe->cond);
		return;
	}
	if(msg.type == tkproto_TakionMessage_PayloadType_DISCONNECT)
	{
		probe_log_disconnect(probe, buf, buf_size);
		probe->state = PROBE_FAILED;
		chiaki_cond_signal(&probe->cond);
		return;
	}
	if(msg.type != tkproto_TakionMessage_PayloadType_STREAMINFO
		|| !msg.has_stream_info_payload)
	{
		CHIAKI_LOGE(probe->log,
			"Stream probe expected STREAMINFO but received protobuf type %d",
			(int)msg.type);
		probe->state = PROBE_FAILED;
		chiaki_cond_signal(&probe->cond);
		return;
	}
	if(audio_header_buf.size != sizeof(audio_header) || !context.profiles
		|| !context.profiles_valid || !context.selected_profile_valid
		|| !context.video_header_size)
	{
		CHIAKI_LOGE(probe->log,
			"Stream probe invalid STREAMINFO: audio=%lu profiles=%u valid=%d candidate_360p=%d",
			(unsigned long)audio_header_buf.size, context.profiles,
			context.profiles_valid ? 1 : 0,
			context.selected_profile_valid ? 1 : 0);
		probe->state = PROBE_FAILED;
		chiaki_cond_signal(&probe->cond);
		return;
	}
	ChiakiAudioHeader output_audio;
	chiaki_audio_header_load(&output_audio, audio_header);
	probe->audio_playback = plus_vita_audio_start(&output_audio, probe->log);
	if(!probe->audio_playback)
	{
		CHIAKI_LOGE(probe->log, "Stream probe audio initialization failed or unsupported header");
		probe->state = PROBE_FAILED;
		chiaki_cond_signal(&probe->cond);
		return;
	}
	probe->video_codec = CHIAKI_CODEC_H264;
	chiaki_bitstream_init(&probe->bitstream, probe->log, probe->video_codec);
	probe->bitstream_ready = chiaki_bitstream_header(&probe->bitstream,
		context.video_header, (unsigned int)context.video_header_size);
	if(!probe->bitstream_ready)
	{
		probe->video_codec = CHIAKI_CODEC_H265;
		chiaki_bitstream_init(&probe->bitstream, probe->log,
			probe->video_codec);
		probe->bitstream_ready = chiaki_bitstream_header(&probe->bitstream,
			context.video_header, (unsigned int)context.video_header_size);
	}
	if(!probe->bitstream_ready)
	{
		CHIAKI_LOGE(probe->log,
			"Stream probe could not parse the first video profile header");
		probe->state = PROBE_FAILED;
		chiaki_cond_signal(&probe->cond);
		return;
	}
	if(probe->video_codec != CHIAKI_CODEC_H264
		|| context.video_header_size > sizeof(probe->video_header))
	{
		CHIAKI_LOGE(probe->log,
			"Stream probe Vita hardware decode requires H.264");
		probe->state = PROBE_FAILED;
		chiaki_cond_signal(&probe->cond);
		return;
	}
	memcpy(probe->video_header, context.video_header,
		context.video_header_size);
	memcpy(probe->original_video_header, context.video_header, context.video_header_size);
	probe->original_video_header_size = context.video_header_size;
	probe->video_header_size = context.video_header_size;
	probe->video_width = context.video_width;
	probe->video_height = context.video_height;
	probe->selected_profile_index = context.selected_profile_index;
	CHIAKI_LOGI(probe->log,
		"Stream probe capture-only profile[%u]=%ux%u",
		probe->selected_profile_index, probe->video_width,
		probe->video_height);
	PlusVitaH264Config h264 = { 0 };
	if(!plus_vita_h264_config(probe->video_header,
		probe->video_header_size, &h264)
		|| !probe_has_nal(probe->video_header, probe->video_header_size, 7)
		|| !probe_has_nal(probe->video_header, probe->video_header_size, 8))
	{
		CHIAKI_LOGE(probe->log,
			"Stream probe rejected malformed or unsupported H.264 SPS");
		probe->state = PROBE_FAILED;
		chiaki_cond_signal(&probe->cond);
		return;
	}
	uint32_t decoder_slots = plus_vita_h264_decoder_slots(&h264);
	if(!decoder_slots || h264.coded_width < probe->video_width
		|| h264.coded_height < probe->video_height)
	{
		CHIAKI_LOGE(probe->log,
			"Stream probe H.264 decoder sizing invalid: coded=%ux%u requested=%ux%u slots=%u",
			h264.coded_width, h264.coded_height, probe->video_width,
			probe->video_height, decoder_slots);
		probe->state = PROBE_FAILED;
		chiaki_cond_signal(&probe->cond);
		return;
	}
	CHIAKI_LOGI(probe->log,
		"Stream probe H.264 SPS: profile=%u level=%u coded=%ux%u refs=%u restriction=%d reorder=%u buffered=%u decoder_slots=%u",
		h264.profile, h264.level, h264.coded_width, h264.coded_height,
		h264.max_ref_frames, h264.has_buffer_restriction ? 1 : 0,
		h264.max_reorder_frames, h264.max_buffered_frames, decoder_slots);
	/* Exact audited headers only; retain originals in the host capture. */
	int decoder_result = 0;
	if(!PROBE_CAPTURE_ONLY)
	{
		uint8_t patched[PROBE_VIDEO_HEADER_MAX];
		size_t patched_size = 0;
		if(!plus_vita_h264_guard_header(probe->video_header, probe->video_header_size,
			patched, sizeof(patched), &patched_size))
		{
			CHIAKI_LOGE(probe->log, "Stream probe LIVE_GUARD_REJECT: parameter sets differ from audited sample");
			probe->state = PROBE_FAILED;
			chiaki_cond_signal(&probe->cond);
			return;
		}
		memcpy(probe->video_header, patched, patched_size);
		probe->video_header_size = patched_size;
		decoder_slots = 5;
		decoder_result = plus_vita_video_decoder_init(&probe->video_decoder,
			probe->log, 1280, 720, decoder_slots);
	}
	if(decoder_result < 0)
	{
		probe->state = PROBE_FAILED;
		chiaki_cond_signal(&probe->cond);
		return;
	}
	probe->video_decoder_ready = !PROBE_CAPTURE_ONLY;
	CHIAKI_LOGI(probe->log,
		"Stream probe decoder configuration: slots=%u original_refs=%u original_buffered=%u",
		decoder_slots, h264.max_ref_frames, h264.max_buffered_frames);
	if(h264.max_ref_frames > decoder_slots
		|| (h264.has_buffer_restriction && h264.max_buffered_frames > decoder_slots))
		CHIAKI_LOGI(probe->log,
			"Stream probe hardware cap is below SPS buffering: declared_refs=%u declared_buffered=%u hardware_slots=%u",
			h264.max_ref_frames, h264.max_buffered_frames, decoder_slots);
	CHIAKI_LOGI(probe->log,
		"Stream probe STREAMINFO validated: audio=%lu profiles=%u video_header=%lu codec=%d dimensions=%ux%u",
		(unsigned long)audio_header_buf.size, context.profiles,
		(unsigned long)context.video_header_size, (int)probe->video_codec,
		probe->video_width, probe->video_height);
	ChiakiErrorCode err = probe_send_streaminfo_ack(probe);
	CHIAKI_LOGI(probe->log, "Stream probe STREAMINFOACK send result=%d",
		(int)err);
	if(err == CHIAKI_ERR_SUCCESS)
	{
		err = probe_send_controller(probe);
		CHIAKI_LOGI(probe->log,
			"Stream probe controller announce result=%d", (int)err);
	}
	if(err == CHIAKI_ERR_SUCCESS)
	{
		err = probe_enable_microphone(probe);
		CHIAKI_LOGI(probe->log,
			"Stream probe microphone enable result=%d", (int)err);
	}
	if(err != CHIAKI_ERR_SUCCESS)
	{
		probe->state = PROBE_FAILED;
		chiaki_cond_signal(&probe->cond);
		return;
	}
	probe->state = PROBE_EXPECT_MEDIA;
	CHIAKI_LOGI(probe->log,
		"Stream probe handoff complete; waiting for audio and video packets");
}

static void probe_takion_cb(ChiakiTakionEvent *event, void *user)
{
	PlusVitaStreamProbe *probe = user;
	uint64_t arrival_us = sceKernelGetProcessTimeWide();
	chiaki_mutex_lock(&probe->mutex);
	uint64_t locked_us = sceKernelGetProcessTimeWide();
	bool measure = probe->latency_collecting && probe->state == PROBE_EXPECT_MEDIA;
	probe->callback_arrival_us = arrival_us;
	if(event->type == CHIAKI_TAKION_EVENT_TYPE_CONNECTED
		&& probe->state == PROBE_CONNECTING)
	{
		CHIAKI_LOGI(probe->log, "Stream probe Takion connected");
		probe->state = PROBE_EXPECT_BANG;
		chiaki_cond_signal(&probe->cond);
	}
	else if(event->type == CHIAKI_TAKION_EVENT_TYPE_DISCONNECT)
	{
		if(probe->state == PROBE_COMPLETE)
			CHIAKI_LOGI(probe->log,
				"Stream probe Takion socket cleanly disconnected");
		else
		{
			CHIAKI_LOGE(probe->log,
				"Stream probe Takion socket disconnected while state=%d",
				(int)probe->state);
			probe->state = PROBE_FAILED;
		}
		chiaki_cond_signal(&probe->cond);
	}
	else if(event->type == CHIAKI_TAKION_EVENT_TYPE_DATA
		&& event->data.data_type == CHIAKI_TAKION_MESSAGE_DATA_TYPE_PROTOBUF)
	{
		if(probe->state == PROBE_EXPECT_BANG)
			probe_handle_bang(probe, event->data.buf, event->data.buf_size);
		else if(probe->state == PROBE_EXPECT_STREAMINFO)
			probe_handle_streaminfo(probe, event->data.buf,
				event->data.buf_size);
		else if(probe->state == PROBE_EXPECT_MEDIA)
		{
			tkproto_TakionMessage msg = { 0 };
			pb_istream_t stream = pb_istream_from_buffer(event->data.buf,
				event->data.buf_size);
			if(pb_decode(&stream, tkproto_TakionMessage_fields, &msg)
				&& msg.type == tkproto_TakionMessage_PayloadType_CONNECTIONQUALITY
				&& msg.has_connection_quality_payload)
			{
				probe->quality_reports++;
				CHIAKI_LOGI(probe->log,
					"Stream probe server quality: target_bitrate=%u upstream_bitrate=%u loss=%lld",
					(unsigned int)msg.connection_quality_payload.target_bitrate,
					(unsigned int)msg.connection_quality_payload.upstream_bitrate,
					(long long)msg.connection_quality_payload.loss);
			}
		}
	}
	else if(event->type == CHIAKI_TAKION_EVENT_TYPE_AV
		&& probe->state == PROBE_EXPECT_MEDIA && event->av)
	{
		ChiakiErrorCode media_err = chiaki_gkcrypt_decrypt(
			probe->crypt_remote,
			event->av->key_pos + CHIAKI_GKCRYPT_BLOCK_SIZE,
			event->av->data, event->av->data_size);
		if(media_err != CHIAKI_ERR_SUCCESS)
		{
			CHIAKI_LOGE(probe->log,
				"Stream probe media decrypt failed: %d", (int)media_err);
			probe->state = PROBE_FAILED;
			chiaki_cond_signal(&probe->cond);
			goto callback_done;
		}
		if(event->av->is_video)
		{
			probe->video_packets++;
			if(probe->video_packets == 1)
				CHIAKI_LOGI(probe->log,
					"Stream probe first video packet received: %lu bytes",
					(unsigned long)event->av->data_size);
			media_err = probe_video_packet(probe, event->av);
		}
		else if(!event->av->is_haptics)
		{
			probe->audio_packets++;
			if(probe->audio_packets == 1)
				CHIAKI_LOGI(probe->log,
					"Stream probe first audio packet received: %lu bytes",
					(unsigned long)event->av->data_size);
			media_err = probe_audio_packet(probe, event->av);
		}
		if(media_err != CHIAKI_ERR_SUCCESS)
		{
			CHIAKI_LOGE(probe->log,
				"Stream probe media frame validation failed: %d",
				(int)media_err);
			probe->state = PROBE_FAILED;
			chiaki_cond_signal(&probe->cond);
		}
	}
callback_done:
	if(measure)
	{
		plus_vita_latency_span(&probe->latency.callback_mutex_wait, arrival_us, locked_us);
		plus_vita_latency_span(&probe->latency.callback_work, locked_us,
			sceKernelGetProcessTimeWide());
	}
	chiaki_mutex_unlock(&probe->mutex);
}

static ChiakiErrorCode probe_send_big(PlusVitaStreamProbe *probe)
{
	uint8_t public_key[128];
	ChiakiPBBuf public_key_buf = { sizeof(public_key), public_key };
	uint8_t signature[32];
	ChiakiPBBuf signature_buf = { sizeof(signature), signature };
	ChiakiErrorCode err = chiaki_ecdh_get_local_pub_key(&probe->ecdh,
		public_key, &public_key_buf.size, probe->handshake_key,
		signature, &signature_buf.size);
	if(err != CHIAKI_ERR_SUCCESS)
		return err;
	tkproto_TakionMessage msg = { 0 };
	msg.type = tkproto_TakionMessage_PayloadType_BIG;
	msg.has_big_payload = true;
	msg.big_payload.client_version = probe->takion.version;
	msg.big_payload.session_key.arg = probe->provision->session_id;
	msg.big_payload.session_key.funcs.encode = chiaki_pb_encode_string;
	msg.big_payload.launch_spec.arg = probe->provision->launch_spec;
	msg.big_payload.launch_spec.funcs.encode = chiaki_pb_encode_string;
	msg.big_payload.encrypted_key.funcs.encode = encode_zero_key;
	msg.big_payload.ecdh_pub_key.arg = &public_key_buf;
	msg.big_payload.ecdh_pub_key.funcs.encode = chiaki_pb_encode_buf;
	msg.big_payload.ecdh_sig.arg = &signature_buf;
	msg.big_payload.ecdh_sig.funcs.encode = chiaki_pb_encode_buf;
	uint8_t buf[32768];
	pb_ostream_t stream = pb_ostream_from_buffer(buf, sizeof(buf));
	if(!pb_encode(&stream, tkproto_TakionMessage_fields, &msg))
		return CHIAKI_ERR_INVALID_DATA;
	uint32_t mtu = probe->provision->mtu_in < probe->provision->mtu_out
		? probe->provision->mtu_in : probe->provision->mtu_out;
	if(mtu <= 58)
		return CHIAKI_ERR_INVALID_DATA;
	mtu -= 28;
	CHIAKI_LOGI(probe->log,
		"Stream probe BIG encoded: %lu bytes, Takion MTU=%u",
		(unsigned long)stream.bytes_written, (unsigned int)mtu);
	size_t remaining = stream.bytes_written;
	size_t pos = 0;
	size_t chunks = 0;
	bool first = true;
	while(remaining + (first ? 30u : 29u) > mtu)
	{
		size_t chunk = mtu - (first ? 30u : 29u);
		err = first
			? chiaki_takion_send_message_data(&probe->takion, 0, 1,
				buf + pos, chunk, NULL)
			: chiaki_takion_send_message_data_cont(&probe->takion, 0, 1,
				buf + pos, chunk, NULL);
		if(err != CHIAKI_ERR_SUCCESS)
		{
			CHIAKI_LOGE(probe->log,
				"Stream probe BIG chunk %lu send failed: %d",
				(unsigned long)(chunks + 1), (int)err);
			return err;
		}
		chunks++;
		first = false;
		pos += chunk;
		remaining -= chunk;
	}
	err = first
		? chiaki_takion_send_message_data(&probe->takion, 1, 1,
			buf + pos, remaining, NULL)
		: chiaki_takion_send_message_data_cont(&probe->takion, 1, 1,
			buf + pos, remaining, NULL);
	if(err != CHIAKI_ERR_SUCCESS)
		CHIAKI_LOGE(probe->log,
			"Stream probe BIG final chunk %lu send failed: %d",
			(unsigned long)(chunks + 1), (int)err);
	else
		CHIAKI_LOGI(probe->log,
			"Stream probe BIG sent in %lu chunk(s)",
			(unsigned long)(chunks + 1));
	return err;
}

/* Called with the probe mutex held; returns with it held. Frame ownership
 * leaves the queue before unlocking. All decoder/display work stays here. */
static ChiakiErrorCode probe_visible_preview(PlusVitaStreamProbe *probe,
	bool *user_stopped)
{
	PlusVitaVideoPresent present;
	ChiakiFeedbackSender controller_feedback;
	bool controller_feedback_ready = false;
#if PLUS_VITA_INPUT_WORKER
	PlusVitaInputWorker *input_worker = NULL;
	PlusVitaInputStats input_stats = { 0 };
#else
	ChiakiControllerState controller_state_prev;
	chiaki_controller_state_set_idle(&controller_state_prev);
#endif
	unsigned int controller_updates = 0;
	chiaki_mutex_unlock(&probe->mutex);
	int native_result = plus_vita_video_present_init(&present);
	SceTouchSamplingState rear_sampling_prev = SCE_TOUCH_SAMPLING_STATE_STOP;
	bool rear_sampling_known = sceTouchGetSamplingState(
		SCE_TOUCH_PORT_BACK, &rear_sampling_prev) >= 0;
	SceTouchPanelInfo rear_info = { 0 };
	int rear_info_result = sceTouchGetPanelInfo(SCE_TOUCH_PORT_BACK, &rear_info);
	int rear_start_result = rear_info_result >= 0
		? sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START)
		: rear_info_result;
	bool rear_ready = rear_info_result >= 0 && rear_start_result >= 0;
	int rear_mid_x = ((int)rear_info.minAaX + (int)rear_info.maxAaX) / 2;
	int rear_mid_y = ((int)rear_info.minAaY + (int)rear_info.maxAaY) / 2;
	SceTouchSamplingState front_sampling_prev = SCE_TOUCH_SAMPLING_STATE_STOP;
	bool front_sampling_known = sceTouchGetSamplingState(
		SCE_TOUCH_PORT_FRONT, &front_sampling_prev) >= 0;
	SceTouchPanelInfo front_info = { 0 };
	int front_info_result = sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &front_info);
	int front_start_result = front_info_result >= 0
		? sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START)
		: front_info_result;
	bool front_ready = front_info_result >= 0 && front_start_result >= 0;
#if PLUS_VITA_INPUT_WORKER
	PlusVitaInputVita input_vita = { .rear_ready = rear_ready, .front_ready = front_ready,
		.rear_mid_x = rear_mid_x, .rear_mid_y = rear_mid_y, .front_info = front_info };
#endif
	probe_preview_lock(probe);
	CHIAKI_LOGI(probe->log, "Stream probe rear touch ready=%d panel_x=%d..%d mid_x=%d panel_y=%d..%d mid_y=%d sampling_prev=%d",
		rear_ready ? 1 : 0, (int)rear_info.minAaX, (int)rear_info.maxAaX,
		rear_mid_x, (int)rear_info.minAaY, (int)rear_info.maxAaY,
		rear_mid_y, rear_sampling_known ? (int)rear_sampling_prev : -1);
	CHIAKI_LOGI(probe->log, "Stream probe front touch ready=%d panel_x=%d..%d panel_y=%d..%d sampling_prev=%d",
		front_ready ? 1 : 0, (int)front_info.minAaX, (int)front_info.maxAaX,
		(int)front_info.minAaY, (int)front_info.maxAaY,
		front_sampling_known ? (int)front_sampling_prev : -1);
	CHIAKI_LOGI(probe->log, "Stream probe preview display init=0x%08x duration_ms=%u",
		native_result, PROBE_PREVIEW_MS);
	CHIAKI_LOGI(probe->log, "Stream probe preview output=direct-avc width=%u height=%u pitch=%u cpu_scale_us=0 queue_limit=%u",
		PLUS_VITA_SCREEN_WIDTH, PLUS_VITA_SCREEN_HEIGHT, PLUS_VITA_SCREEN_PITCH, PROBE_QUEUE_MAX);
	ChiakiErrorCode err = native_result < 0 ? CHIAKI_ERR_UNKNOWN : CHIAKI_ERR_SUCCESS;
	uint64_t started = sceKernelGetProcessTimeWide() / 1000u;
#if !PLUS_VITA_INPUT_WORKER
	uint64_t stop_chord_since = 0;
#endif
	uint64_t last_feedback = started;
	uint64_t first_display = 0, last_display = 0;
	unsigned int displayed = 0;
	unsigned int decoded = 0, presentation_skips = 0;
	unsigned int recovery_generation_fed = 0;
	uint64_t prepare_sum = 0, decode_sum = 0, decode_max = 0;
	uint64_t display_sum = 0, wait_sum = 0, wait_max = 0, queue_wait_max = 0;
#if !PLUS_VITA_INPUT_WORKER
	uint64_t last_input_poll_us = 0;
#endif
	probe->latency_collecting = true;
	plus_vita_diagnostic_stream_timing_begin();
	while(err == CHIAKI_ERR_SUCCESS)
	{
		uint64_t now = sceKernelGetProcessTimeWide() / 1000u;
		if(probe->state == PROBE_EXPECT_MEDIA)
		{
			if(!controller_feedback_ready)
			{
				memset(&controller_feedback, 0, sizeof(controller_feedback));
				chiaki_mutex_unlock(&probe->mutex);
				ChiakiErrorCode feedback_err = chiaki_feedback_sender_init(
					&controller_feedback, &probe->takion);
				probe_preview_lock(probe);
				if(feedback_err != CHIAKI_ERR_SUCCESS)
				{
					CHIAKI_LOGE(probe->log, "Stream probe controller feedback init failed=%d", (int)feedback_err);
					err = feedback_err;
					break;
				}
				controller_feedback_ready = true;
				CHIAKI_LOGI(probe->log, "Stream probe Vita gameplay input enabled rear_L2_R2_L3_R3=%d front_touchpad=%d",
					rear_ready ? 1 : 0, front_ready ? 1 : 0);
#if PLUS_VITA_INPUT_WORKER
				chiaki_mutex_unlock(&probe->mutex);
				feedback_err = plus_vita_input_worker_start(&input_worker, &controller_feedback,
					plus_vita_input_vita_read, &input_vita);
				probe_preview_lock(probe);
				if(feedback_err != CHIAKI_ERR_SUCCESS) { err = feedback_err; break; }
				CHIAKI_LOGI(probe->log, "Stream probe independent input started period_us=%u control_transition_logs=off aggregate_counters=on",
					PLUS_VITA_INPUT_PERIOD_US);
#endif
			}
#if PLUS_VITA_INPUT_WORKER
			PlusVitaInputStatus input_status = plus_vita_input_worker_status(input_worker);
			if(input_status.done)
			{
				if(input_status.error != CHIAKI_ERR_SUCCESS) { err = input_status.error; break; }
				if(!input_status.user_stopped) { err = CHIAKI_ERR_THREAD; break; }
				probe->state = PROBE_COMPLETE;
				if(user_stopped) *user_stopped = true;
				CHIAKI_LOGI(probe->log, "Stream probe user stop requested after %llu ms; releasing controller and disconnecting",
					(unsigned long long)(now - started));
				break;
			}
#else
			SceCtrlData pad = { 0 };
			uint64_t sampled_us = sceKernelGetProcessTimeWide();
			if(last_input_poll_us)
				plus_vita_latency_span(&probe->latency.input_poll_gap,
					last_input_poll_us, sampled_us);
			last_input_poll_us = sampled_us;
			if(sceCtrlPeekBufferPositive(0, &pad, 1) > 0)
			{
				const uint32_t stop_buttons = SCE_CTRL_START | SCE_CTRL_SELECT;
				if((pad.buttons & stop_buttons) == stop_buttons)
				{
					if(!stop_chord_since) stop_chord_since = now;
					/* Do not forward the exit chord as Options + Share. */
					pad.buttons &= ~stop_buttons;
					if(now - stop_chord_since >= PROBE_STOP_HOLD_MS)
					{
						ChiakiControllerState idle;
						chiaki_controller_state_set_idle(&idle);
						chiaki_mutex_unlock(&probe->mutex);
						ChiakiErrorCode feedback_err =
							chiaki_feedback_sender_set_controller_state(
								&controller_feedback, &idle);
						probe_preview_lock(probe);
						if(feedback_err != CHIAKI_ERR_SUCCESS)
						{
							err = feedback_err;
							break;
						}
						controller_state_prev = idle;
						probe->state = PROBE_COMPLETE;
						if(user_stopped) *user_stopped = true;
						CHIAKI_LOGI(probe->log,
							"Stream probe user stop requested after %llu ms; releasing controller and disconnecting",
							(unsigned long long)(now - started));
						/* set_controller_state wakes the sender thread. Give it a
						 * chance to transmit the idle state before joining it. */
						chiaki_mutex_unlock(&probe->mutex);
						sceKernelDelayThread(100000);
						probe_preview_lock(probe);
						break;
					}
				}
				else
					stop_chord_since = 0;
				ChiakiControllerState state;
				plus_vita_input_map_pad(&pad, &state);
				if(rear_ready)
				{
					SceTouchData rear_touch = { 0 };
					if(sceTouchPeek(SCE_TOUCH_PORT_BACK, &rear_touch, 1) > 0)
						plus_vita_input_map_rear(&rear_touch, rear_mid_x,
							rear_mid_y, &state);
				}
				if(front_ready)
				{
					SceTouchData front_touch = { 0 };
					if(sceTouchPeek(SCE_TOUCH_PORT_FRONT, &front_touch, 1) > 0)
						plus_vita_input_map_front(&front_touch, &front_info,
							&controller_state_prev, &state);
				}
				if(!chiaki_controller_state_equals(&state, &controller_state_prev))
				{
					chiaki_mutex_unlock(&probe->mutex);
					uint64_t publish_started_us = sceKernelGetProcessTimeWide();
					ChiakiErrorCode feedback_err = plus_vita_feedback_sender_set_controller_state_sampled(
						&controller_feedback, &state, sampled_us);
					plus_vita_latency_span(&probe->latency.input_publish_call,
						publish_started_us, sceKernelGetProcessTimeWide());
					probe_preview_lock(probe);
					if(feedback_err != CHIAKI_ERR_SUCCESS)
					{
						CHIAKI_LOGE(probe->log, "Stream probe controller feedback failed=%d", (int)feedback_err);
						err = feedback_err;
						break;
					}
					if(state.buttons != controller_state_prev.buttons)
						CHIAKI_LOGI(probe->log, "Stream probe controller buttons=0x%04x", (unsigned int)state.buttons);
					if(state.l2_state != controller_state_prev.l2_state
						|| state.r2_state != controller_state_prev.r2_state)
						CHIAKI_LOGI(probe->log, "Stream probe rear triggers L2=%u R2=%u",
							(unsigned int)state.l2_state, (unsigned int)state.r2_state);
					unsigned int touch_count = plus_vita_input_touch_count(&state);
					if(touch_count != plus_vita_input_touch_count(&controller_state_prev))
						CHIAKI_LOGI(probe->log, "Stream probe front touch contacts=%u click=%d",
							touch_count, (state.buttons & CHIAKI_CONTROLLER_BUTTON_TOUCHPAD) ? 1 : 0);
					controller_state_prev = state;
					controller_updates++;
				}
			}
			else
			{
				probe->latency.input_poll_failures++;
				stop_chord_since = 0;
			}
#endif
		}
		if(probe->state == PROBE_FAILED)
		{
			err = CHIAKI_ERR_INVALID_RESPONSE;
			break;
		}
		if(probe->state == PROBE_EXPECT_MEDIA && now - last_feedback >= 200)
		{
			probe_reception_flush(probe, false);
			/* Match upstream's 200ms congestion report using completed frame
			 * generations. Count FEC and source units once, never manufacture
			 * loss for profiles intentionally not submitted to our decoder. */
			ChiakiTakionCongestionPacket feedback = { 0 };
			feedback.received = probe->feedback_pending_received > UINT16_MAX
				? UINT16_MAX : (uint16_t)probe->feedback_pending_received;
			feedback.lost = probe->feedback_pending_lost > UINT16_MAX
				? UINT16_MAX : (uint16_t)probe->feedback_pending_lost;
			probe->feedback_pending_received -= feedback.received;
			probe->feedback_pending_lost -= feedback.lost;
			chiaki_mutex_unlock(&probe->mutex);
			ChiakiErrorCode feedback_err = chiaki_takion_send_congestion(
				&probe->takion, &feedback);
			probe_preview_lock(probe);
			probe->feedback_reports++;
			if(probe->feedback_reports <= 3 || probe->feedback_reports % 10 == 0
				|| feedback_err != CHIAKI_ERR_SUCCESS)
				CHIAKI_LOGI(probe->log,
					"Stream probe congestion report=%u received=%u lost=%u send=%d",
					probe->feedback_reports, (unsigned int)feedback.received,
					(unsigned int)feedback.lost, (int)feedback_err);
			last_feedback = now;
			if(feedback_err != CHIAKI_ERR_SUCCESS)
			{
				err = feedback_err;
				break;
			}
		}
		/* Input and congestion sends temporarily release the mutex. The
		 * receive callback can advance recovery timestamps during those sends,
		 * so sample the clock again while holding the mutex before deadlines. */
		now = sceKernelGetProcessTimeWide() / 1000u;
		if(probe->state == PROBE_EXPECT_MEDIA)
		{
			err = probe_video_drain(probe, false);
			if(err != CHIAKI_ERR_SUCCESS) break;
			/* Draining can request recovery and advance its timestamps. */
			now = sceKernelGetProcessTimeWide() / 1000u;
		}
#if PLUS_VITA_DIAGNOSTIC_APP
		if(now - started >= PROBE_TIMEOUT_MS + PROBE_PREVIEW_MS)
		{
			CHIAKI_LOGE(probe->log, "Stream probe bounded preview deadline expired");
			err = CHIAKI_ERR_TIMEOUT;
			break;
		}
#endif
		if(probe->awaiting_recovery_idr
			&& now >= probe->recovery_started_ms
			&& now - probe->recovery_started_ms >= 5000)
		{
			CHIAKI_LOGE(probe->log, "Stream probe recovery IDR_TIMEOUT after 5000ms; dependent frames not decoded");
			err = CHIAKI_ERR_TIMEOUT;
			break;
		}
#if PLUS_VITA_DIAGNOSTIC_APP
		if(first_display && now - first_display >= PROBE_PREVIEW_MS
			&& !probe->awaiting_recovery_idr && !probe->recovery_output_pending)
		{
			if(displayed >= 2 && probe->audio_frames && now - last_display < 1000
				&& (!PROBE_RECOVERY_FAULT_TEST || (probe->recovery_fault_injected && probe->recovery_resumes)))
				probe->state = PROBE_COMPLETE;
			else
				err = CHIAKI_ERR_TIMEOUT;
			break;
		}
#endif
		/* Keep actual last_display truthful. A validated recovery IDR gets a
		 * fresh output deadline, not the stale deadline from before the freeze. */
		uint64_t output_deadline_base = last_display;
		if(probe->recovery_resumed_ms > output_deadline_base)
			output_deadline_base = probe->recovery_resumed_ms;
		uint64_t output_idle_ms = now >= output_deadline_base
			? now - output_deadline_base : 0;
		if((!first_display && now - started >= PROBE_TIMEOUT_MS)
			|| (first_display && !probe->awaiting_recovery_idr && output_idle_ms >= 3000))
		{
			CHIAKI_LOGE(probe->log,
				"Stream probe preview frame deadline expired started=%d output_idle_ms=%llu recovery_output_pending=%d queue=%u",
				first_display ? 1 : 0, (unsigned long long)output_idle_ms,
				probe->recovery_output_pending ? 1 : 0, probe->queue_count);
			err = CHIAKI_ERR_TIMEOUT;
			break;
		}
		if(!probe->queue_count)
		{
			/* Keep controller polling responsive even when video pauses while
			 * waiting for a lost frame or a recovery IDR. */
			err = chiaki_cond_timedwait(&probe->cond, &probe->mutex, 8);
			if(err == CHIAKI_ERR_TIMEOUT)
				err = CHIAKI_ERR_SUCCESS;
			continue;
		}
		ProbeQueuedFrame frame = probe->queue[probe->queue_head];
		memset(&probe->queue[probe->queue_head], 0, sizeof(frame));
		probe->queue_head = (probe->queue_head + 1) % PROBE_QUEUE_MAX;
		probe->queue_count--;
		probe->video_decode_attempts++;
		if(frame.recovery_generation) recovery_generation_fed = frame.recovery_generation;
		uint64_t dequeued_us = sceKernelGetProcessTimeWide();
		uint64_t queue_wait_us = dequeued_us - frame.enqueued_us;
		if(queue_wait_us > queue_wait_max)
			queue_wait_max = queue_wait_us;
		plus_vita_latency_span(&probe->latency.queue_wait, frame.enqueued_us, dequeued_us);
		chiaki_mutex_unlock(&probe->mutex);
		bool produced = false;
		native_result = plus_vita_video_decoder_decode(&probe->video_decoder,
			probe->video_header, frame.include_header ? probe->video_header_size : 0,
			frame.data, frame.size, plus_vita_video_present_back_buffer(&present),
			PLUS_VITA_SCREEN_PITCH, PLUS_VITA_SCREEN_WIDTH, PLUS_VITA_SCREEN_HEIGHT,
			&produced);
		free(frame.data);
		prepare_sum += probe->video_decoder.last_prepare_us;
		decode_sum += probe->video_decoder.last_decode_us;
		if(probe->video_decoder.last_decode_us > decode_max)
			decode_max = probe->video_decoder.last_decode_us;
		plus_vita_latency_add(&probe->latency.decode_prepare, probe->video_decoder.last_prepare_us);
		plus_vita_latency_add(&probe->latency.decode, probe->video_decoder.last_decode_us);
		probe_preview_lock(probe);
		if(native_result < 0)
		{
			CHIAKI_LOGE(probe->log, "Stream probe preview decode failed=0x%08x", native_result);
			err = CHIAKI_ERR_UNKNOWN;
			break;
		}
		if(produced)
		{
			probe->video_decoded_frame = true;
			decoded++;
			/* Preserve every compressed reference frame. Only discard an
			 * already-decoded display update when catching up; its back
			 * buffer stays writable and will be replaced by the next decode. */
			bool recovery_output = probe->recovery_output_pending
				&& recovery_generation_fed == probe->recovery_resumes;
			bool show = recovery_output || !displayed || probe->queue_count < 2
				|| sceKernelGetProcessTimeWide() / 1000u - last_display >= 100;
			if(show)
			{
				chiaki_mutex_unlock(&probe->mutex);
				native_result = plus_vita_video_present_frame(&present);
				display_sum += present.last_display_us;
				wait_sum += present.last_wait_us;
				if(present.last_wait_us > wait_max)
					wait_max = present.last_wait_us;
				plus_vita_latency_add(&probe->latency.display_wait, present.last_wait_us);
				uint64_t display_done_us = sceKernelGetProcessTimeWide();
				if(native_result >= 0)
				{
					/* Native output can belong to an earlier AU. These spans
					 * describe this submission, not the age of that picture. */
					plus_vita_latency_span(&probe->latency.submitted_first_packet_to_present,
						frame.first_packet_us, display_done_us);
					plus_vita_latency_span(&probe->latency.submitted_ready_to_present,
						frame.ready_us, display_done_us);
				}
				probe_preview_lock(probe);
				if(native_result < 0)
				{
					CHIAKI_LOGE(probe->log, "Stream probe preview display failed=0x%08x", native_result);
					err = CHIAKI_ERR_UNKNOWN;
					break;
				}
				displayed++;
				last_display = sceKernelGetProcessTimeWide() / 1000u;
				if(recovery_output && recovery_generation_fed == probe->recovery_resumes)
				{
					probe->recovery_output_pending = false;
					CHIAKI_LOGI(probe->log, "Stream probe recovery output displayed generation=%u after_idr_ms=%llu",
						recovery_generation_fed, (unsigned long long)(last_display - probe->recovery_resumed_ms));
				}
				if(!first_display)
					first_display = last_display;
			}
			else
				presentation_skips++;
		}
		else if(!displayed && probe->video_decode_attempts >= PROBE_DECODE_ATTEMPTS_MAX)
		{
			CHIAKI_LOGE(probe->log, "Stream probe AVC_NO_OUTPUT after %u submissions", probe->video_decode_attempts);
			err = CHIAKI_ERR_TIMEOUT;
		}
		if(probe->video_decode_attempts <= 6 || probe->video_decode_attempts % 60 == 0)
			CHIAKI_LOGI(probe->log, "Stream probe timing submitted=%u decoded=%u displayed=%u skipped=%u prepare_us=%llu decode_us=%llu queue=%u queue_wait_us=%llu elapsed_ms=%llu",
				probe->video_decode_attempts, decoded, displayed, presentation_skips,
				(unsigned long long)probe->video_decoder.last_prepare_us,
				(unsigned long long)probe->video_decoder.last_decode_us,
				probe->queue_count, (unsigned long long)queue_wait_us,
				(unsigned long long)(first_display ? sceKernelGetProcessTimeWide() / 1000u - first_display : 0));
	}
	/* Stop accepting media before restoring/freeing display buffers. */
	if(err != CHIAKI_ERR_SUCCESS)
		probe->state = PROBE_FAILED;
	probe->latency_collecting = false;
	chiaki_mutex_unlock(&probe->mutex);
#if PLUS_VITA_INPUT_WORKER
	if(input_worker)
	{
		ChiakiErrorCode input_err = plus_vita_input_worker_stop(input_worker, &input_stats);
		if(err == CHIAKI_ERR_SUCCESS && input_err != CHIAKI_ERR_SUCCESS) err = input_err;
		probe->latency.input_poll_gap = input_stats.poll_gap;
		probe->latency.input_publish_call = input_stats.publish_call;
		probe->latency.input_poll_failures = input_stats.poll_failures;
		controller_updates = (unsigned int)input_stats.updates;
	}
#endif
	if(controller_feedback_ready)
	{
		chiaki_feedback_sender_fini(&controller_feedback);
		plus_vita_feedback_latency_get_stopped(&probe->latency.feedback);
	}
	plus_vita_diagnostic_stream_timing_end(&probe->latency.log_write_call,
		&probe->latency.log_mutex_wait);
#if PLUS_VITA_INPUT_WORKER
	CHIAKI_LOGI(probe->log,
		"Stream probe independent input summary: period_us=%u updates=%llu poll_failures=%llu missed_deadlines=%llu native_changes=%llu native_repeats=%llu neutral_result=%d neutral_consumed=%d",
		PLUS_VITA_INPUT_PERIOD_US, (unsigned long long)input_stats.updates,
		(unsigned long long)input_stats.poll_failures, (unsigned long long)input_stats.missed_deadlines,
		(unsigned long long)input_stats.native_changes, (unsigned long long)input_stats.native_repeats,
		(int)input_stats.neutral_result, input_stats.neutral_consumed ? 1 : 0);
#endif
	if(rear_ready && rear_sampling_known
		&& rear_sampling_prev != SCE_TOUCH_SAMPLING_STATE_START)
		sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, rear_sampling_prev);
	if(front_ready && front_sampling_known
		&& front_sampling_prev != SCE_TOUCH_SAMPLING_STATE_START)
		sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, front_sampling_prev);
	CHIAKI_LOGI(probe->log, "Stream probe controller updates=%u", controller_updates);
	/* No callback can initialize or use the decoder after the terminal
	 * state above. Retire it before releasing its borrowed output buffers. */
	plus_vita_video_decoder_fini(&probe->video_decoder);
	native_result = plus_vita_video_present_fini(&present);
	probe_preview_lock(probe);
	if(native_result < 0 || err != CHIAKI_ERR_SUCCESS)
	{
		if(native_result < 0) err = CHIAKI_ERR_UNKNOWN;
		probe->state = PROBE_FAILED;
	}
	CHIAKI_LOGI(probe->log, "Stream probe preview summary: displayed=%u submitted=%u complete=%u incomplete=%u stale=%u queue_peak=%u audio=%u restore=0x%08x result=%d",
		displayed, probe->video_decode_attempts, probe->video_frames,
		probe->incomplete_frames, probe->stale_packets, probe->queue_peak,
		probe->audio_frames, native_result, (int)err);
	CHIAKI_LOGI(probe->log, "Stream probe recovery summary: episodes=%u dropped_dependent=%u awaiting_idr=%d validated_idrs=%u",
		probe->recovery_count, probe->recovery_dropped, probe->awaiting_recovery_idr ? 1 : 0, probe->reference_guard.idr_count);
	CHIAKI_LOGI(probe->log, "Stream probe FEC telemetry: advertised_frames=%u parity_packets_seen=%u incomplete_frames=%u reconstruction=on",
		probe->fec_frames_advertised, probe->fec_packets_seen,
		probe->incomplete_frames);
	CHIAKI_LOGI(probe->log,
		"Stream probe FEC summary: attempts=%u repaired_frames=%u repaired_units=%u failed=%u repair_total_us=%llu repair_max_us=%llu",
		probe->fec_attempts, probe->fec_repaired_frames, probe->fec_repaired_units,
		probe->fec_failed, (unsigned long long)probe->fec_total_us,
		(unsigned long long)probe->fec_max_us);
	CHIAKI_LOGI(probe->log, "Stream probe recovery test: injected=%d resumes=%u enabled=%d duration_ms=%u",
		probe->recovery_fault_injected ? 1 : 0, probe->recovery_resumes, PROBE_RECOVERY_FAULT_TEST, PROBE_PREVIEW_MS);
	CHIAKI_LOGI(probe->log,
		"Stream probe reorder summary: slots=%u grace_us=%u peak=%u late_packets=%u saved_frames=%u expired_partial=%u missing_frames=%u pressure_drops=%u retired_packets=%u waits=%u wait_total_us=%llu wait_max_us=%llu ready_hold_total_us=%llu ready_hold_max_us=%llu",
		PLUS_VITA_VIDEO_REORDER_SLOTS, PLUS_VITA_VIDEO_REORDER_GRACE_US,
		probe->reorder_peak, probe->reorder_late_packets, probe->reorder_saved_frames,
		probe->reorder_expired_frames, probe->reorder_missing_frames,
		probe->reorder_pressure_drops, probe->reorder_retired_packets, probe->reorder_waits,
		(unsigned long long)probe->reorder_wait_total_us,
		(unsigned long long)probe->reorder_wait_max_us,
		(unsigned long long)probe->reorder_held_total_us,
		(unsigned long long)probe->reorder_held_max_us);
	CHIAKI_LOGI(probe->log,
		"Stream probe observed video profile packets: index0=%u index1=%u index2=%u other=%u selected=%u",
		probe->profile_packet_counts[0], probe->profile_packet_counts[1],
		probe->profile_packet_counts[2],
		probe->profile_packet_counts[3] + probe->profile_packet_counts[4]
			+ probe->profile_packet_counts[5] + probe->profile_packet_counts[6]
			+ probe->profile_packet_counts[7],
		probe->selected_profile_index);
	if(probe->video_packets && probe->selected_profile_index < 8
		&& !probe->profile_packet_counts[probe->selected_profile_index])
		CHIAKI_LOGE(probe->log,
			"Stream probe selected 360p candidate was never sent during preview");
	CHIAKI_LOGI(probe->log, "Stream probe timing summary: decoded=%u skipped=%u prepare_total_us=%llu decode_total_us=%llu decode_max_us=%llu",
		decoded, presentation_skips, (unsigned long long)prepare_sum,
		(unsigned long long)decode_sum, (unsigned long long)decode_max);
	CHIAKI_LOGI(probe->log, "Stream probe display timing: cpu_scale_us=0 submit_total_us=%llu vblank_total_us=%llu vblank_max_us=%llu queue_wait_max_us=%llu",
		(unsigned long long)display_sum, (unsigned long long)wait_sum,
		(unsigned long long)wait_max, (unsigned long long)queue_wait_max);
	return err;
}

/* Called with the probe mutex held. Disk I/O occurs only after Takion closes. */
static ChiakiErrorCode probe_capture_wait(PlusVitaStreamProbe *probe)
{
	uint64_t started = sceKernelGetProcessTimeWide() / 1000u;
	uint64_t last_feedback = started;
	while(probe->state != PROBE_FAILED && !probe->capture_done)
	{
		uint64_t now = sceKernelGetProcessTimeWide() / 1000u;
		if(now - started >= PROBE_TIMEOUT_MS) break;
		if(probe->capture_frames && now - probe->capture_started >= PROBE_CAPTURE_MS)
		{
			probe->capture_done = true;
			break;
		}
		if(probe->state == PROBE_EXPECT_MEDIA && now - last_feedback >= 200)
		{
			ChiakiTakionCongestionPacket feedback = { 0 };
			feedback.received = probe->feedback_pending_received > UINT16_MAX ? UINT16_MAX : (uint16_t)probe->feedback_pending_received;
			feedback.lost = probe->feedback_pending_lost > UINT16_MAX ? UINT16_MAX : (uint16_t)probe->feedback_pending_lost;
			probe->feedback_pending_received -= feedback.received;
			probe->feedback_pending_lost -= feedback.lost;
			chiaki_mutex_unlock(&probe->mutex);
			ChiakiErrorCode err = chiaki_takion_send_congestion(&probe->takion, &feedback);
			chiaki_mutex_lock(&probe->mutex);
			probe->feedback_reports++;
			last_feedback = now;
			if(err != CHIAKI_ERR_SUCCESS) { probe->state = PROBE_FAILED; return err; }
		}
		ChiakiErrorCode err = chiaki_cond_timedwait(&probe->cond, &probe->mutex, 100);
		if(err != CHIAKI_ERR_SUCCESS && err != CHIAKI_ERR_TIMEOUT) return err;
	}
	bool valid = probe->state != PROBE_FAILED && probe->capture_done
		&& !probe->capture_gap && probe->capture_frames >= 2;
	CHIAKI_LOGI(probe->log, "Stream probe H264 capture summary: frames=%u bytes=%lu gap=%d incomplete=%u complete=%d decoder=NOT_STARTED",
		probe->capture_frames, (unsigned long)probe->capture_size,
		probe->capture_gap ? 1 : 0, probe->incomplete_frames, valid ? 1 : 0);
	probe->state = valid ? PROBE_COMPLETE : PROBE_FAILED;
	return valid ? CHIAKI_ERR_SUCCESS : CHIAKI_ERR_INVALID_DATA;
}

static uint32_t replay_u32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
		| ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* This experiment accepts only the exact offline-audited sample, never live
 * or arbitrary video. Host software decode proved all 300 frames identical
 * after changing only SPS reference/buffering declarations from nine to five. */
static ChiakiErrorCode probe_replay(ChiakiLog *log)
{
	CHIAKI_LOGI(log, "Stream probe build=%s mode=%s live_transport=NOT_STARTED", PLUS_VITA_STREAM_PROBE_BUILD,
		PLUS_VITA_STREAM_PROBE_REPLAY ? "OFFLINE_REPLAY" : "LIVE_GUARD_SELFTEST");
	FILE *file = fopen("ux0:data/plus-vita-replay-720p.bin", "rb");
	if(!file) { CHIAKI_LOGE(log, "Stream probe replay file missing"); return CHIAKI_ERR_INVALID_DATA; }
	if(fseek(file, 0, SEEK_END) != 0) { fclose(file); return CHIAKI_ERR_INVALID_DATA; }
	long length = ftell(file);
	if(length < 28 || (unsigned long)length > PROBE_CAPTURE_BYTES_MAX || fseek(file, 0, SEEK_SET) != 0)
	{ fclose(file); return CHIAKI_ERR_INVALID_DATA; }
	uint8_t *data = malloc((size_t)length);
	if(!data) { fclose(file); return CHIAKI_ERR_MEMORY; }
	bool loaded = fread(data, 1, (size_t)length, file) == (size_t)length;
	if(fclose(file) != 0) loaded = false;
	ChiakiErrorCode err = CHIAKI_ERR_INVALID_DATA;
	uint8_t digest[32];
	char hex[65];
	const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
	if(!loaded || !md || mbedtls_md(md, data, (size_t)length, digest) != 0) goto done;
	for(unsigned int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", digest[i]);
	if(strcmp(hex, "ea735a32bcc166a8d375830dbebddfff08c58db47fad7b47c6453b043786b802") != 0)
	{ CHIAKI_LOGE(log, "Stream probe replay SHA256 mismatch; rejecting unaudited input"); goto done; }
	if(memcmp(data, "PVREPL01", 8) || replay_u32(data + 8) != 1280
		|| replay_u32(data + 12) != 720 || replay_u32(data + 16) != 5
		|| replay_u32(data + 20) != 300) goto done;
	/* Validate every record before creating hardware resources. */
	size_t pos = 28, payload = 0;
	for(unsigned int i = 0; i < 300; i++)
	{
		if((size_t)length - pos < 4) goto done;
		uint32_t size = replay_u32(data + pos); pos += 4;
		if(!size || size > PROBE_FRAME_BYTES_MAX || size > (size_t)length - pos) goto done;
		payload += size; pos += size;
	}
	if(pos != (size_t)length || payload != replay_u32(data + 24)) goto done;
	CHIAKI_LOGI(log, "Stream probe replay audit accepted: pictures=300 input=1280x720 slots=5 host_frame_hash_matches=300");
	/* Run the live slice guard against all audited AUs before any experiment.
	 * Only the first record's known SPS/PPS are removed for this parser test. */
	PlusVitaH264Guard guard = { 0 };
	pos = 28;
	for(unsigned int i = 0; i < 300; i++)
	{
		uint32_t size = replay_u32(data + pos); pos += 4;
		size_t offset = 0;
		if(!i)
		{
			while(offset + 4 < size)
			{
				size_t prefix = data[pos + offset] == 0 && data[pos + offset + 1] == 0
					&& data[pos + offset + 2] == 1 ? 3 :
					(data[pos + offset] == 0 && data[pos + offset + 1] == 0
					&& data[pos + offset + 2] == 0 && data[pos + offset + 3] == 1 ? 4 : 0);
				if(prefix && (data[pos + offset + prefix] & 31) == 5) break;
				offset++;
			}
		}
		PlusVitaH264Guard before = guard;
		if(offset >= size || !plus_vita_h264_guard_frame(&guard, data + pos + offset, size - offset))
		{ CHIAKI_LOGE(log, "Stream probe guard self-test rejected audited AU=%u", i); goto done; }
		/* Duplicate P frame must fail; a complete short-term IDR can reset
		 * frame_num. Duplicate IDR wire packets are filtered separately. */
		PlusVitaH264Guard duplicate = guard, truncated = before;
		bool accepted = plus_vita_h264_guard_frame(&duplicate, data + pos + offset, size - offset);
		if(accepted != (i == 0) || (i == 0 && duplicate.idr_count != 2)
			|| plus_vita_h264_guard_frame(&truncated, data + pos + offset, 5)) goto done;
		pos += size;
	}
	if(!plus_vita_h264_guard_selftest() || !probe_wire_next(UINT16_MAX, 0)
		|| probe_wire_next(42, 42) || probe_wire_next(42, 44) || probe_wire_next(42, 41)) goto done;
	CHIAKI_LOGI(log, "Stream probe guard self-test passed: 300 AUs; validated IDR reset accepted; duplicate P/truncated frames and changed/truncated headers rejected");
	if(!PLUS_VITA_STREAM_PROBE_REPLAY) { err = CHIAKI_ERR_SUCCESS; goto done; }
	PlusVitaVideoPresent present;
	int native = plus_vita_video_present_init(&present);
	if(native < 0) { err = CHIAKI_ERR_UNKNOWN; goto done; }
	PlusVitaVideoDecoder decoder = { .decoder_block = -1 };
	native = plus_vita_video_decoder_init(&decoder, log, 1280, 720, 5);
	unsigned int submitted = 0, displayed = 0;
	uint64_t started = sceKernelGetProcessTimeWide(), decode_us = 0, max_us = 0;
	pos = 28;
	err = native < 0 ? CHIAKI_ERR_UNKNOWN : CHIAKI_ERR_SUCCESS;
	while(err == CHIAKI_ERR_SUCCESS && submitted < 300)
	{
		if(sceKernelGetProcessTimeWide() - started > 15000000u) { err = CHIAKI_ERR_TIMEOUT; break; }
		uint32_t size = replay_u32(data + pos); pos += 4;
		bool produced = false;
		native = plus_vita_video_decoder_decode(&decoder, NULL, 0, data + pos, size,
			plus_vita_video_present_back_buffer(&present), PLUS_VITA_SCREEN_PITCH,
			PLUS_VITA_SCREEN_WIDTH, PLUS_VITA_SCREEN_HEIGHT, &produced);
		pos += size; submitted++;
		decode_us += decoder.last_decode_us;
		if(decoder.last_decode_us > max_us) max_us = decoder.last_decode_us;
		if(native < 0) { err = CHIAKI_ERR_UNKNOWN; break; }
		if(produced)
		{
			native = plus_vita_video_present_frame(&present);
			if(native < 0) { err = CHIAKI_ERR_UNKNOWN; break; }
			displayed++;
		}
	}
	/* Drain with a strict bound, including a final zero-output call. Never
	 * count buffered frames as displayed until scanout succeeds. */
	bool drain_complete = false;
	for(unsigned int attempt = 0; err == CHIAKI_ERR_SUCCESS && attempt < 17; attempt++)
	{
		if(sceKernelGetProcessTimeWide() - started > 15000000u) { err = CHIAKI_ERR_TIMEOUT; break; }
		bool produced = false;
		native = plus_vita_video_decoder_drain(&decoder,
			plus_vita_video_present_back_buffer(&present), PLUS_VITA_SCREEN_PITCH,
			PLUS_VITA_SCREEN_WIDTH, PLUS_VITA_SCREEN_HEIGHT, &produced);
		if(native < 0) { err = CHIAKI_ERR_UNKNOWN; break; }
		if(!produced) { drain_complete = true; break; }
		if(displayed >= submitted) { err = CHIAKI_ERR_INVALID_RESPONSE; break; }
		native = plus_vita_video_present_frame(&present);
		if(native < 0) { err = CHIAKI_ERR_UNKNOWN; break; }
		displayed++;
	}
	if(err == CHIAKI_ERR_SUCCESS && (!drain_complete || displayed != 300)) err = CHIAKI_ERR_INVALID_RESPONSE;
	CHIAKI_LOGI(log, "Stream probe replay summary: submitted=%u displayed=%u native=0x%08x decode_total_us=%llu decode_max_us=%llu result=%d",
		submitted, displayed, native, (unsigned long long)decode_us,
		(unsigned long long)max_us, (int)err);
	plus_vita_video_decoder_fini(&decoder);
	native = plus_vita_video_present_fini(&present);
	if(native < 0) err = CHIAKI_ERR_UNKNOWN;
done:
	free(data);
	return err;
}

ChiakiErrorCode plus_vita_stream_probe_with_pump(const ChiakiCloudProvisionResult *provision,
	ChiakiLog *log, bool *user_stopped, bool (*connect_pump)(void *), void *pump_user)
{
	if(user_stopped) *user_stopped = false;
	if(PLUS_VITA_STREAM_PROBE_REPLAY) return probe_replay(log);
	ChiakiErrorCode guard_test = probe_replay(log);
	if(guard_test != CHIAKI_ERR_SUCCESS) return guard_test;
	if(!plus_vita_video_fec_selftest())
	{
		CHIAKI_LOGE(log, "Stream probe FEC self-test failed");
		return CHIAKI_ERR_FEC_FAILED;
	}
	CHIAKI_LOGI(log, "Stream probe FEC self-test passed: two missing source units restored with exact bytes and padding lengths");
	if(!provision || !provision->handshake_key || !provision->launch_spec
		|| !provision->session_id || provision->server_port <= 0)
		return CHIAKI_ERR_INVALID_DATA;
	PlusVitaStreamProbe probe = { .log = log, .provision = provision,
		.state = PROBE_CONNECTING };
	CHIAKI_LOGI(log, "Stream probe build=%s", PLUS_VITA_STREAM_PROBE_BUILD);
	const uint8_t idr3[] = { 0, 0, 1, 0x65 };
	const uint8_t idr4[] = { 0, 0, 0, 1, 0x65 };
	const uint8_t non_idr[] = { 0, 0, 1, 0x41, 0x65 };
	if(!probe_has_nal(idr3, sizeof(idr3), 5)
		|| !probe_has_nal(idr4, sizeof(idr4), 5)
		|| probe_has_nal(non_idr, sizeof(non_idr), 5)
		|| probe_has_nal(idr3, sizeof(idr3) - 1, 5))
		return CHIAKI_ERR_INVALID_DATA;
	CHIAKI_LOGI(log, "Stream probe H264 capture Annex-B IDR detector self-test passed");
	probe.video_decoder.decoder_block = -1;
	size_t key_size = sizeof(probe.handshake_key);
	ChiakiErrorCode err = chiaki_base64_decode(provision->handshake_key,
		strlen(provision->handshake_key), probe.handshake_key, &key_size);
	if(err != CHIAKI_ERR_SUCCESS || key_size != sizeof(probe.handshake_key))
		return CHIAKI_ERR_INVALID_DATA;
	if((err = chiaki_mutex_init(&probe.mutex, false)) != CHIAKI_ERR_SUCCESS)
		goto done_key;
	if((err = chiaki_cond_init(&probe.cond)) != CHIAKI_ERR_SUCCESS)
		goto done_mutex;
	if(PROBE_CAPTURE_ONLY)
	{
		probe.capture_data = malloc(PROBE_CAPTURE_BYTES_MAX);
		if(!probe.capture_data) { err = CHIAKI_ERR_MEMORY; goto done_cond; }
	}
	/* Match Chiaki's production cloud stream path: PS Now/Cloud Takion uses
	 * secp256k1 here. Both supported curves serialize to 65 bytes, so using
	 * P-256/secp256r1 only fails later when the server's BANG point is read. */
	if((err = chiaki_ecdh_init(&probe.ecdh)) != CHIAKI_ERR_SUCCESS)
		goto done_cond;
	struct sockaddr_in address = { 0 };
	address.sin_family = AF_INET;
	address.sin_port = htons((uint16_t)provision->server_port);
	if(inet_pton(AF_INET, provision->server_ip, &address.sin_addr) != 1)
	{
		err = CHIAKI_ERR_PARSE_ADDR;
		goto done_ecdh;
	}
	ChiakiTakionConnectInfo info = { 0 };
	info.log = log;
	info.sa = (struct sockaddr *)&address;
	info.sa_len = sizeof(address);
	info.ip_dontfrag = false;
	info.cb = probe_takion_cb;
	info.cb_user = &probe;
	info.disable_audio_video = CHIAKI_NONE_DISABLED;
	info.enable_crypt = true;
	info.enable_dualsense = false;
	info.protocol_version = 9;
	info.close_socket = true;
	info.service_type = CHIAKI_SERVICE_TYPE_PSNOW;
	info.psn_wrapper_type = provision->psn_wrapper_type;
	info.is_ping_handshake = false;
	err = chiaki_takion_connect(&probe.takion, &info, NULL);
	if(err != CHIAKI_ERR_SUCCESS)
		goto done_ecdh;
	chiaki_mutex_lock(&probe.mutex);
	uint64_t connect_started = sceKernelGetProcessTimeWide();
	while(probe.state == PROBE_CONNECTING)
	{
		if(connect_pump)
		{
			chiaki_mutex_unlock(&probe.mutex);
			bool keep_connecting = connect_pump(pump_user);
			chiaki_mutex_lock(&probe.mutex);
			if(!keep_connecting) { err = CHIAKI_ERR_CANCELED; break; }
		}
		uint64_t elapsed_ms = (sceKernelGetProcessTimeWide() - connect_started) / 1000u;
		if(elapsed_ms >= PROBE_TIMEOUT_MS) { err = CHIAKI_ERR_TIMEOUT; break; }
		uint64_t remaining_ms = PROBE_TIMEOUT_MS - elapsed_ms;
		uint64_t wait_ms = connect_pump && remaining_ms > 100u ? 100u : remaining_ms;
		err = chiaki_cond_timedwait(&probe.cond, &probe.mutex, wait_ms);
		if(err != CHIAKI_ERR_SUCCESS && err != CHIAKI_ERR_TIMEOUT) break;
		if(err == CHIAKI_ERR_TIMEOUT && connect_pump) err = CHIAKI_ERR_SUCCESS;
		else if(err == CHIAKI_ERR_TIMEOUT) break;
	}
	if(err == CHIAKI_ERR_SUCCESS && probe.state == PROBE_EXPECT_BANG)
	{
		CHIAKI_LOGI(log, "Stream probe connected; sending BIG");
		err = probe_send_big(&probe);
	}
	if(err == CHIAKI_ERR_SUCCESS)
		err = PROBE_CAPTURE_ONLY ? probe_capture_wait(&probe)
			: probe_visible_preview(&probe, user_stopped);
	CHIAKI_LOGI(log, "Stream probe wait completed: err=%d state=%d",
		(int)err, (int)probe.state);
	CHIAKI_LOGI(log, "Stream probe feedback summary: sent=%u server_quality=%u",
		probe.feedback_reports, probe.quality_reports);
	if(err == CHIAKI_ERR_TIMEOUT && probe.video_decode_attempts
		&& !probe.video_decoded_frame)
		CHIAKI_LOGE(log, "Stream probe AVC_NO_OUTPUT before deadline: submissions=%u",
			probe.video_decode_attempts);
	if(err == CHIAKI_ERR_SUCCESS && probe.state != PROBE_COMPLETE)
		err = CHIAKI_ERR_INVALID_RESPONSE;
	if(err == CHIAKI_ERR_SUCCESS || probe.video_decoder_ready)
	{
		ChiakiErrorCode disconnect_err = probe_send_disconnect(&probe,
			user_stopped && *user_stopped ? "Client Disconnecting"
				: "Probe Complete");
		CHIAKI_LOGI(log, "Stream probe disconnect send result=%d",
			(int)disconnect_err);
		if(err == CHIAKI_ERR_SUCCESS && disconnect_err != CHIAKI_ERR_SUCCESS)
			err = disconnect_err;
	}
	chiaki_mutex_unlock(&probe.mutex);
	chiaki_takion_close(&probe.takion);
	if(!PROBE_CAPTURE_ONLY) probe_latency_dump(&probe);
	if(PROBE_CAPTURE_ONLY && probe.capture_size)
	{
		char path[128];
		snprintf(path, sizeof(path), "ux0:data/plus-vita-capture-%llu-%s.h264",
			(unsigned long long)sceKernelGetProcessTimeWide(),
			err == CHIAKI_ERR_SUCCESS && !probe.capture_gap ? "complete" : "partial");
		FILE *file = fopen(path, "wb");
		bool saved = false;
		if(file)
		{
			saved = fwrite(probe.capture_data, 1, probe.capture_size, file) == probe.capture_size;
			if(fclose(file) != 0) saved = false;
		}
		CHIAKI_LOGI(log, "Stream probe H264 capture file=%s saved=%d bytes=%lu (video only; no auth or transport headers)", path, saved ? 1 : 0, (unsigned long)probe.capture_size);
		if(!saved) { remove(path); err = CHIAKI_ERR_UNKNOWN; }
	}
	for(unsigned int i = 0; i < PROBE_QUEUE_MAX; i++)
		free(probe.queue[i].data);
	for(unsigned int i = 0; i < PLUS_VITA_VIDEO_REORDER_SLOTS; i++)
		probe_video_assembly_reset(&probe.video_assembly[i]);
	chiaki_gkcrypt_free(probe.crypt_remote);
	chiaki_gkcrypt_free(probe.crypt_local);
done_ecdh:
	plus_vita_video_decoder_fini(&probe.video_decoder);
	chiaki_ecdh_fini(&probe.ecdh);
done_cond:
	if(!plus_vita_audio_stop(probe.audio_playback) && err == CHIAKI_ERR_SUCCESS)
		err = CHIAKI_ERR_INVALID_DATA;
	free(probe.capture_data);
	chiaki_cond_fini(&probe.cond);
done_mutex:
	chiaki_mutex_fini(&probe.mutex);
done_key:
	memset(probe.handshake_key, 0, sizeof(probe.handshake_key));
	return err;
}

ChiakiErrorCode plus_vita_stream_probe(const ChiakiCloudProvisionResult *provision,
	ChiakiLog *log, bool *user_stopped)
{
	return plus_vita_stream_probe_with_pump(provision, log, user_stopped, NULL, NULL);
}
