// SPDX-License-Identifier: AGPL-3.0-only

#include <chiaki/random.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __vita__
#include <psp2/kernel/rng.h>
#else
#include <errno.h>
#include <sys/random.h>
#endif

ChiakiErrorCode chiaki_random_bytes_crypt(uint8_t *buf, size_t buf_size)
{
	if(!buf && buf_size)
		return CHIAKI_ERR_INVALID_DATA;

	size_t offset = 0;
	while(offset < buf_size)
	{
#ifdef __vita__
		/* sceKernelGetRandomNumber accepts at most 64 bytes per call. */
		size_t chunk = buf_size - offset;
		if(chunk > 64)
			chunk = 64;
		if(sceKernelGetRandomNumber(buf + offset, chunk) < 0)
			return CHIAKI_ERR_UNKNOWN;
		offset += chunk;
#else
		ssize_t written = getrandom(buf + offset, buf_size - offset, 0);
		if(written < 0)
		{
			if(errno == EINTR)
				continue;
			return CHIAKI_ERR_UNKNOWN;
		}
		if(written == 0)
			return CHIAKI_ERR_UNKNOWN;
		offset += (size_t)written;
#endif
	}
	return CHIAKI_ERR_SUCCESS;
}

uint32_t chiaki_random_32(void)
{
	uint32_t value = 0;
	(void)chiaki_random_bytes_crypt((uint8_t *)&value, sizeof(value));
	return value;
}
