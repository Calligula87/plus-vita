// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_BUILD_IDENTITY_H
#define PLUS_VITA_BUILD_IDENTITY_H
#if PLUS_VITA_RELEASE_CANDIDATE
#define PLUS_VITA_APP_BUILD "v000"
#define PLUS_VITA_BUILD_LABEL "v000"
#define PLUS_VITA_ABOUT_STATUS "v000"
#else
#define PLUS_VITA_APP_BUILD "recovery-diagnostics-1"
#define PLUS_VITA_BUILD_LABEL "DEVELOPMENT BUILD"
#define PLUS_VITA_ABOUT_STATUS "Development build. Not a public release."
#endif
#if PLUS_VITA_INPUT_WORKER && PLUS_VITA_DEFERRED_LOG
#define PLUS_VITA_DIAGNOSTIC_PROFILE "720p-input-worker-deferred-log"
#elif PLUS_VITA_INPUT_WORKER
#define PLUS_VITA_DIAGNOSTIC_PROFILE "720p-input-worker-sync-log"
#elif PLUS_VITA_DEFERRED_LOG
#define PLUS_VITA_DIAGNOSTIC_PROFILE "720p-main-input-deferred-log"
#else
#define PLUS_VITA_DIAGNOSTIC_PROFILE "720p-main-input-sync-log"
#endif
#endif
