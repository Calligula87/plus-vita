// SPDX-License-Identifier: AGPL-3.0-only
/* External HTTP/DNS fixture for UI journeys without artwork requests. */
#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <psp2/net/net.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#undef curl_easy_setopt
#undef curl_easy_getinfo
#include "cover_http_fixture.h"
