// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_BUILD_IDENTITY_H
#define PLUS_VITA_BUILD_IDENTITY_H
#if PLUS_VITA_PS3_LIVE
#define PLUS_VITA_APP_BUILD "ps3-live-54-2"
#define PLUS_VITA_BUILD_LABEL "PS3 PLAYBACK #54-2"
#define PLUS_VITA_ABOUT_STATUS "v001 PS3 playback candidate."
#elif PLUS_VITA_PS3_REPLAY
#define PLUS_VITA_APP_BUILD "ps3-replay-53"
#define PLUS_VITA_BUILD_LABEL "PS3 REPLAY #53"
#define PLUS_VITA_ABOUT_STATUS "Audited offline PS3 decoder replay."
#elif PLUS_VITA_PS3_STARTUP_SAMPLE
#define PLUS_VITA_APP_BUILD "ps3-media-start-52"
#define PLUS_VITA_BUILD_LABEL "PS3 MEDIA START #52"
#define PLUS_VITA_ABOUT_STATUS "Diagnostic build for PS3 media startup."
#elif PLUS_VITA_RELEASE_CANDIDATE
#define PLUS_VITA_APP_BUILD "v000"
#define PLUS_VITA_BUILD_LABEL "v000"
#define PLUS_VITA_ABOUT_STATUS "v000"
#else
#define PLUS_VITA_APP_BUILD "scrolling-menus-41-42-43-1"
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
