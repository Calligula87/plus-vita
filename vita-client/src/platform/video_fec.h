// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <chiaki/common.h>

/* Reconstruct missing source units, including their two-byte padding header.
 * units/sizes contain source slots followed by parity slots; missing slots are
 * NULL/zero. On success, newly allocated source slots belong to the caller.
 * On failure every input slot is unchanged. No partial frame is returned. */
ChiakiErrorCode plus_vita_video_fec_repair(uint8_t **units, size_t *sizes,
	unsigned source_count, unsigned parity_count, unsigned *repaired);

/* Small fixed Cauchy fixture independent of the backend's encoder. */
bool plus_vita_video_fec_selftest(void);
