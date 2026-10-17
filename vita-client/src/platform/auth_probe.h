// SPDX-License-Identifier: AGPL-3.0-only

#ifndef PLUS_VITA_AUTH_PROBE_H
#define PLUS_VITA_AUTH_PROBE_H

#include <stdbool.h>

#include "credential.h"

#define PLUS_VITA_CATALOG_NAME_MAX 96
#define PLUS_VITA_CATALOG_ID_MAX 64

typedef struct plus_vita_catalog_entry_t
{
	char name[PLUS_VITA_CATALOG_NAME_MAX];
	char id[PLUS_VITA_CATALOG_ID_MAX];
	char platform[8];
	bool launch_id_valid;
} PlusVitaCatalogEntry;

typedef struct plus_vita_auth_probe_result_t
{
	bool attempted;
	bool format_valid;
	bool transport_ok;
	bool authorized;
	bool credential_rejected;
	bool session_transport_ok;
	bool session_created;
	bool store_transport_ok;
	bool store_resolved;
	bool native_catalog_complete;
	bool owned_authorized;
	bool owned_complete;
	char online_id[129]; /* Runtime-only authenticated identity; never log/persist. */
	long http_status;
	long session_http_status;
	long store_http_status;
	char country[8];
	char language[8];
	char store_country[8];
	char store_language[8];
	unsigned int native_category_count;
	unsigned int native_title_count;
	unsigned int owned_page_count;
	unsigned int owned_entitlement_count;
	unsigned int owned_game_count;
	PlusVitaCatalogEntry *native_titles;
	unsigned int native_titles_count;
} PlusVitaAuthProbeResult;

void plus_vita_auth_probe(const PlusVitaCredential *credential,
	PlusVitaAuthProbeResult *result);
void plus_vita_auth_probe_result_fini(PlusVitaAuthProbeResult *result);

#endif
