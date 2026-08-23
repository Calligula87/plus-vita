// SPDX-License-Identifier: AGPL-3.0-only

#ifndef PLUS_VITA_CREDENTIAL_H
#define PLUS_VITA_CREDENTIAL_H

#include <stdbool.h>
#include <stddef.h>

#define PLUS_VITA_CREDENTIAL_MAX 256

typedef struct plus_vita_credential_t
{
	char value[PLUS_VITA_CREDENTIAL_MAX + 1];
	size_t length;
} PlusVitaCredential;

void plus_vita_credential_init(PlusVitaCredential *credential);
bool plus_vita_credential_set(PlusVitaCredential *credential,
	const char *value, size_t length);
void plus_vita_credential_clear(PlusVitaCredential *credential);
bool plus_vita_credential_is_clear(const PlusVitaCredential *credential);
void plus_vita_credential_redacted(const PlusVitaCredential *credential,
	char *out, size_t out_size);

#endif
