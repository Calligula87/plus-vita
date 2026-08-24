// SPDX-License-Identifier: AGPL-3.0-only

#ifndef PLUS_VITA_AUTH_PROBE_H
#define PLUS_VITA_AUTH_PROBE_H

#include <stdbool.h>

#include "credential.h"

typedef struct plus_vita_auth_probe_result_t
{
	bool attempted;
	bool format_valid;
	bool transport_ok;
	bool authorized;
	bool session_transport_ok;
	bool session_created;
	long http_status;
	long session_http_status;
	char country[8];
	char language[8];
} PlusVitaAuthProbeResult;

void plus_vita_auth_probe(const PlusVitaCredential *credential,
	PlusVitaAuthProbeResult *result);

#endif
