// SPDX-License-Identifier: AGPL-3.0-only

#ifndef PLUS_VITA_PROVISION_PROBE_H
#define PLUS_VITA_PROVISION_PROBE_H

#include <stdbool.h>

#include "credential.h"

typedef struct plus_vita_provision_probe_result_t
{
	bool attempted;
	bool transport_ok;
	bool authorized;
	long http_status;
} PlusVitaProvisionProbeResult;

void plus_vita_provision_authorize(const PlusVitaCredential *credential,
	PlusVitaProvisionProbeResult *result);

#endif
