// SPDX-License-Identifier: AGPL-3.0-only

#ifndef PLUS_VITA_STREAM_PROBE_H
#define PLUS_VITA_STREAM_PROBE_H

#include <chiaki/cloudsession.h>
#include <chiaki/log.h>

#define PLUS_VITA_STREAM_PROBE_BUILD "live-720p-refs5-video-reorder-1"
#define PLUS_VITA_STREAM_PROBE_REPLAY 0

ChiakiErrorCode plus_vita_stream_probe(const ChiakiCloudProvisionResult *provision,
	ChiakiLog *log, bool *user_stopped);

#endif
