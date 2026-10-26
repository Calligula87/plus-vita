// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_BUILD_IDENTITY_H
#define PLUS_VITA_BUILD_IDENTITY_H
#if PLUS_VITA_RELEASE_CANDIDATE
#define PLUS_VITA_APP_BUILD "v0.1.0-beta-candidate-2"
#define PLUS_VITA_BUILD_LABEL "BETA CANDIDATE"
#define PLUS_VITA_ABOUT_STATUS "v0.1.0 beta candidate.\nFinal Vita release validation is still required."
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
