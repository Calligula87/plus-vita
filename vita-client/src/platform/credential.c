// SPDX-License-Identifier: AGPL-3.0-only

#include "credential.h"

#include <stdio.h>
#include <string.h>

static void secure_clear(void *data, size_t size)
{
	volatile unsigned char *p = data;
	while(size--)
		*p++ = 0;
}

void plus_vita_credential_init(PlusVitaCredential *credential)
{
	if(credential)
		secure_clear(credential, sizeof(*credential));
}

bool plus_vita_credential_set(PlusVitaCredential *credential,
	const char *value, size_t length)
{
	if(!credential || !value || length == 0
		|| length > PLUS_VITA_CREDENTIAL_MAX)
		return false;
	for(size_t i = 0; i < length; i++)
	{
		unsigned char c = (unsigned char)value[i];
		if(c <= 0x20 || c >= 0x7f)
			return false;
	}
	secure_clear(credential, sizeof(*credential));
	memcpy(credential->value, value, length);
	credential->value[length] = 0;
	credential->length = length;
	return true;
}

void plus_vita_credential_clear(PlusVitaCredential *credential)
{
	if(credential)
		secure_clear(credential, sizeof(*credential));
}

bool plus_vita_credential_is_clear(const PlusVitaCredential *credential)
{
	if(!credential)
		return false;
	const unsigned char *p = (const unsigned char *)credential;
	for(size_t i = 0; i < sizeof(*credential); i++)
		if(p[i] != 0)
			return false;
	return true;
}

void plus_vita_credential_redacted(const PlusVitaCredential *credential,
	char *out, size_t out_size)
{
	if(!out || out_size == 0)
		return;
	snprintf(out, out_size, "<credential:%u bytes>",
		credential ? (unsigned int)credential->length : 0);
}
