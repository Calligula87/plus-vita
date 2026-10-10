// SPDX-License-Identifier: AGPL-3.0-only

#include <chiaki/gkcrypt.h>

#include <stdlib.h>
#include <malloc.h>

const char *chiaki_error_string(ChiakiErrorCode code)
{
	switch(code)
	{
		case CHIAKI_ERR_SUCCESS: return "Success";
		case CHIAKI_ERR_NETWORK: return "Network error";
		case CHIAKI_ERR_TIMEOUT: return "Timeout";
		case CHIAKI_ERR_CANCELED: return "Canceled";
		case CHIAKI_ERR_INVALID_DATA: return "Invalid data";
		default: return "Unknown";
	}
}

const char *chiaki_service_type_string(ChiakiServiceType service_type)
{
	switch(service_type)
	{
		case CHIAKI_SERVICE_TYPE_PSNOW: return "psnow";
		case CHIAKI_SERVICE_TYPE_PSCLOUD: return "pscloud";
		default: return "remote_play";
	}
}

void *chiaki_aligned_alloc(size_t alignment, size_t size)
{
	return memalign(alignment, size);
}

void chiaki_aligned_free(void *ptr)
{
	free(ptr);
}
