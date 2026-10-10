// SPDX-License-Identifier: AGPL-3.0-only

#include "catalog_fetch_guard.h"
#include "cloudcatalog_internal.h"

#include <chiaki/random.h>

#include <stdio.h>
#include <string.h>

static unsigned int attempts;

void plus_vita_catalog_fetch_guard_reset(void)
{
	attempts = 0;
}

unsigned int plus_vita_catalog_fetch_guard_attempts(void)
{
	return attempts;
}

CCNativeResult cc_fetch_psnow_native(ChiakiLog *log, const char *npsso,
	struct json_object **out_games, char *out_country, size_t cc_sz,
	char *out_language, size_t lang_sz, char *out_store_country,
	size_t store_cc_sz, char *out_store_lang, size_t store_lang_sz,
	bool *out_complete)
{
	(void)log; (void)npsso;
	attempts++;
	if(out_games) *out_games = NULL;
	if(out_country && cc_sz) out_country[0] = 0;
	if(out_language && lang_sz) out_language[0] = 0;
	if(out_store_country && store_cc_sz) out_store_country[0] = 0;
	if(out_store_lang && store_lang_sz) out_store_lang[0] = 0;
	if(out_complete) *out_complete = false;
	return CC_NATIVE_FATAL;
}

bool cc_fetch_imagic(ChiakiLog *log, const char *stored_locale,
	CCImagicResult *out)
{
	(void)log; (void)stored_locale;
	attempts++;
	if(out) memset(out, 0, sizeof(*out));
	return false;
}

void cc_imagic_result_fini(CCImagicResult *result)
{
	if(!result) return;
	if(result->browse) json_object_put(result->browse);
	if(result->supplement) json_object_put(result->supplement);
	if(result->aliases) json_object_put(result->aliases);
	memset(result, 0, sizeof(*result));
}

CCOwnedResult cc_fetch_owned(ChiakiLog *log, const char *npsso,
	struct json_object **out_games, struct json_object **out_component_ids)
{
	(void)log; (void)npsso;
	attempts++;
	if(out_games) *out_games = NULL;
	if(out_component_ids) *out_component_ids = NULL;
	return CC_OWNED_ERROR;
}

ChiakiErrorCode chiaki_holepunch_generate_client_device_uid(
	char *out, size_t *out_size)
{
	if(!out || !out_size || *out_size < 49)
		return CHIAKI_ERR_BUF_TOO_SMALL;
	uint8_t random_bytes[16];
	ChiakiErrorCode err = chiaki_random_bytes_crypt(random_bytes,
		sizeof(random_bytes));
	if(err != CHIAKI_ERR_SUCCESS)
		return err;
	snprintf(out, *out_size, "0000000700410080");
	for(size_t i = 0; i < sizeof(random_bytes); i++)
		snprintf(out + 16 + i * 2, 3, "%02x", random_bytes[i]);
	volatile uint8_t *wipe = random_bytes;
	for(size_t i = 0; i < sizeof(random_bytes); i++)
		wipe[i] = 0;
	*out_size = 48;
	return CHIAKI_ERR_SUCCESS;
}
