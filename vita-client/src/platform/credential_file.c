// SPDX-License-Identifier: AGPL-3.0-only

#include "credential_file.h"

#include <psp2/io/fcntl.h>

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#define CREDENTIAL_FILE "ux0:data/plus-vita-auth.json"
#define CREDENTIAL_FILE_MAX 4096

static void secure_clear(void *value, size_t size)
{
	volatile unsigned char *p = value;
	while(size--)
		*p++ = 0;
}

static bool memory_is_clear(const void *value, size_t size)
{
	const unsigned char *p = value;
	while(size--)
		if(*p++)
			return false;
	return true;
}

static bool parse_npsso(const char *json, size_t size, PlusVitaCredential *credential)
{
	static const char key[] = "\"npsso\"";
	const char *end = json + size;
	const char *key_pos = NULL;
	for(const char *p = json; p + sizeof(key) - 1 <= end; p++)
		if(memcmp(p, key, sizeof(key) - 1) == 0)
		{
			key_pos = p + sizeof(key) - 1;
			break;
		}
	if(!key_pos)
		return false;
	while(key_pos < end && isspace((unsigned char)*key_pos)) key_pos++;
	if(key_pos == end || *key_pos++ != ':') return false;
	while(key_pos < end && isspace((unsigned char)*key_pos)) key_pos++;
	if(key_pos == end || *key_pos++ != '"') return false;
	const char *value = key_pos;
	while(key_pos < end && *key_pos != '"')
	{
		if(!isalnum((unsigned char)*key_pos)) return false;
		key_pos++;
	}
	return key_pos < end && (size_t)(key_pos - value) == 64
		&& plus_vita_credential_set(credential, value, 64);
}

void plus_vita_credential_file_read(PlusVitaCredential *credential,
	PlusVitaCredentialFileResult *result)
{
	char buffer[CREDENTIAL_FILE_MAX + 1] = { 0 };
	memset(result, 0, sizeof(*result));
	plus_vita_credential_init(credential);
	SceUID file = sceIoOpen(CREDENTIAL_FILE, SCE_O_RDONLY, 0);
	if(file < 0)
	{
		secure_clear(buffer, sizeof(buffer));
		result->buffer_wiped = memory_is_clear(buffer, sizeof(buffer));
		return;
	}
	result->found = true;
	int read_size = sceIoRead(file, buffer, CREDENTIAL_FILE_MAX);
	sceIoClose(file);
	if(read_size > 0)
		result->parsed = parse_npsso(buffer, (size_t)read_size, credential);
	secure_clear(buffer, sizeof(buffer));
	result->buffer_wiped = memory_is_clear(buffer, sizeof(buffer));
	if(result->parsed)
		result->retained = true;
	else
	{
		/* Do not leave a malformed credential file blocking a replacement. */
		sceIoRemove(CREDENTIAL_FILE);
		plus_vita_credential_clear(credential);
	}
}
