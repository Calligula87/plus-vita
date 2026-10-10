// SPDX-License-Identifier: AGPL-3.0-only

#ifndef PLUS_VITA_CREDENTIAL_FILE_H
#define PLUS_VITA_CREDENTIAL_FILE_H

#include <stdbool.h>

#include "credential.h"

typedef struct plus_vita_credential_file_result_t
{
	bool found;
	bool parsed;
	bool retained;
	bool buffer_wiped;
} PlusVitaCredentialFileResult;

void plus_vita_credential_file_read(PlusVitaCredential *credential,
	PlusVitaCredentialFileResult *result);

#endif
