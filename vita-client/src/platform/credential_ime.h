// SPDX-License-Identifier: AGPL-3.0-only

#ifndef PLUS_VITA_CREDENTIAL_IME_H
#define PLUS_VITA_CREDENTIAL_IME_H

#include <stdbool.h>
#include <stddef.h>

#include "credential.h"

typedef struct plus_vita_credential_ime_result_t
{
	bool initialized;
	bool accepted;
	bool validated;
	bool credential_wiped;
	bool input_wiped;
	size_t entered_length;
} PlusVitaCredentialImeResult;

void plus_vita_credential_ime_read(PlusVitaCredential *credential,
	PlusVitaCredentialImeResult *result);
void plus_vita_credential_ime_read_masked(PlusVitaCredential *credential,
	PlusVitaCredentialImeResult *result);
void plus_vita_account_label_read(char output[129], PlusVitaCredentialImeResult *result);
void plus_vita_catalogue_search_read(char output[129], PlusVitaCredentialImeResult *result);

#endif
