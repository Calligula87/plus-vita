// SPDX-License-Identifier: AGPL-3.0-only

#ifndef PLUS_VITA_STREAM_PROBE_H
#define PLUS_VITA_STREAM_PROBE_H

#include <chiaki/cloudsession.h>
#include <chiaki/log.h>

#if PLUS_VITA_INPUT_WORKER && PLUS_VITA_DEFERRED_LOG
#define PLUS_VITA_STREAM_PROBE_BUILD "live-720p-refs5-input-worker-deferred-log-1"
#define PLUS_VITA_STREAM_PROBE_MODE "input-worker+deferred-log"
#elif PLUS_VITA_INPUT_WORKER
#define PLUS_VITA_STREAM_PROBE_BUILD "live-720p-refs5-input-worker-1"
#define PLUS_VITA_STREAM_PROBE_MODE "input-worker"
#elif PLUS_VITA_DEFERRED_LOG
#define PLUS_VITA_STREAM_PROBE_BUILD "live-720p-refs5-deferred-log-1"
#define PLUS_VITA_STREAM_PROBE_MODE "deferred-log"
#else
#define PLUS_VITA_STREAM_PROBE_BUILD "live-720p-refs5-latency-baseline-1"
#define PLUS_VITA_STREAM_PROBE_MODE "baseline"
#endif
#define PLUS_VITA_STREAM_PROBE_REPLAY 0

ChiakiErrorCode plus_vita_stream_probe(const ChiakiCloudProvisionResult *provision,
	ChiakiLog *log, bool *user_stopped);

#endif
