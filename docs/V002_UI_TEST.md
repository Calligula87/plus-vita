# v002 menu polish - hardware acceptance pending

The implementation for #45, #46 and #47 is local development work against the
accepted v001 baseline at `5351470`. This document records a test procedure,
not a hardware acceptance or a release. Preserve the accepted v001 package and
hash in SCROLLING_MENUS_TEST.md. Do not publish or replace it.

## Implemented presentation

- The cached blue picture pans over a slightly enlarged crop at 20 Hz. The
  1024x576 overscan cache (2.25 MiB) is prepared once at load; frames use row
  copies and no per-pixel wave/time calculations. The static original remains
  available for reduced motion or if the optional cache allocation fails.
  Long blocking intervals advance by one tick without a catch-up jump.
- Content fades for approximately 150 ms over the scene. Branding and header
  remain visible. Focus/scroll/cover updates do not restart the screen fade.
- Settings > Appearance & Sound saves reduced motion, music mute and volume
  (10 percent steps) independently of account and control files. Defaults are
  motion enabled, music unmuted at 20 percent. A failed save is reported and
  the current run remains usable; selecting a setting retries persistence.
- Reduced motion uses the static background, immediate navigation and no
  periodic decorative repaint. It does not mute music.
- Quiet Blue is an original 24-second mono ambient loop with a CC0 source and
  rights notice. It is loaded once from bounded local PCM resources. No network
  requests, samples or Sony recordings are involved.
- The music worker owns a separate MAIN output port during menus. It stops
  before Play allocation/connection, pauses for keyboards/sleep, and restarts
  gently only in eligible foreground menus. The last 512-sample block fades
  down before drain/release/join. At 48 kHz each output block represents 10.67 ms;
  actual backend/scheduler stop latency must be measured on Vita. There is no
  separate fade sleep in the menu thread. Stream playback code/policy is unchanged.

## Software verification

### Device report and correction, 2026-11-06

#### Visual rework after test 2

The retrieved test-2 log identifies `v002-ui-polish-45-46-47-2` and records
274 draws totaling 18,802,442 us (68.6 ms average), with a 242,357 us maximum.
Sampled input-to-present reached 316,993 us. The old effect recomputed time
division/modulo and light bands across the full screen, twice during fades.

Test 3 removes that light-band effect. A 64-pixel horizontal crop excursion
moves the existing enlarged picture over a 6.4-second back-and-forth cycle,
without exposing empty edges. Header/logo and menu coordinates stay fixed.
Fades blend against the same cached rows using packed 8-bit integer arithmetic.
This is an animated picture, not a video player. Sound is unchanged in this
visual-only iteration; the maintainer rejected the existing tonal soundtrack.

The real renderer test first failed against the old implementation because
source-image pixels were not moving. It now verifies known crop coordinates,
movement, static reduced motion and restoration, header continuity and fades.
The same 60-frame instrumented host workload took 406.30 ms with the old
renderer and 64.61 ms with the rework. Host ratios do not establish Vita frame
timing: compare the next device log and get visual acceptance before adoption.

The first uploaded test showed Appearance & Sound but no visible effects or
music. FTP inspection of the presentation record found reduced motion enabled,
music unmuted and volume 100. Reduced motion intentionally disables both visual
effects. The installed audio resource was present.

The audio fixture previously accepted a 480-sample output length, whereas the
VitaSDK contract requires a multiple of 64. Enforcing that contract made the
actual playback regression fail at port creation. A 512-sample buffer passes
the same app/audio lifecycle test. A compile-time granularity check prevents
reintroducing this invalid buffer size. Stream playback is unchanged.

Test 2 clarifies the reduced-motion label with its effect on animations. The
real UI-flow journey now checks that switching reduced motion off restores
both timed background updates and transitions. Device listening and visual
acceptance of the corrected package remain pending.

Use `sh tools/test-host-suite.sh` in the supported WSL environment. The suite
includes all host shell journeys, strict warnings, ASan/UBSan and deterministic
audio reproduction/signal checks. Publication-gate fixtures run when
`PLUS_VITA_TEST_VPK` names an authorized candidate VPK; otherwise the runner
reports them as skipped. Package-only branding checks likewise require that
separately authorized candidate.

Software checks on 2026-11-05 passed all 18 host shell journeys, deterministic
audio reproduction/signal validation and compilation of all six changed Vita
production objects. Recovery used the native-generated preflight source because
WSL lacks CMake. Package-gate fixtures remain pending an authorized v002 VPK;
no package was created. Fresh independent Standards and Spec reviews completed:
no remaining material findings after adding app-level audio lifecycle tests.

If WSL does not provide CMake, use the production preflight source generated by
the native SDK configuration: set `PLUS_VITA_CLOUD_SESSION_TEST_SOURCE` to
`build/vita-feedback/generated/cloudsession_vita.c` when running the host suite.
Regenerate that source through the native configuration before testing changes
to preflight generation.

The main behavioral seam is the real app dispatcher, controlled-clock UI flow
and renderer. Tests cover settings/relaunch, invalid/trailing/truncated records,
failed saves, static/live frames, immediate reduced-motion input, continuous
branding, transition pixels and retained gesture protection. The menu-audio
test uses the actual worker with a platform-output fixture and verifies audible
output, mute, restart and exclusive resource release before returning.

Compile changed production Vita objects with the existing SDK as well as host
tests. No public version constant or numeric Vita revision is changed by this work.

## Vita comparison checklist

When a candidate build is authorized, record its development identity and SHA256
before installation. Compare with the preserved accepted v001 package:

- [ ] Existing splash, logo, wordmark, palette, text and menu positions remain familiar.
- [ ] Browse setup, Settings/help, control mapping, catalogue/details, errors and
      confirmations; effect contrast never obscures text, focus or cover previews.
- [ ] Scroll/drag quickly and compare input/present/draw diagnostics and perceived
      response. Reduce effect complexity if responsiveness regresses.
- [ ] Navigate rapidly with D-pad/stick/Back and touch-release/cancel. No stale
      activation, queued transitions or whole-screen black flash occurs.
- [ ] Reduced motion immediately freezes decoration and removes navigation waits;
      idle screens no longer repaint periodically. Reload the app to check persistence.
- [ ] Listen on headphones and speakers through several loops. Check calmness,
      comfortable default level, no clicks/clipping, and an unobtrusive loop boundary.
- [ ] Change volume, mute and reduced motion; relaunch and verify all choices.
- [ ] Play/stop repeatedly; cancel allocation/connection and exercise a failed
      launch. Menu audio never overlaps game audio or leaves duplicate workers.
- [ ] Open/close the search/account keyboards repeatedly. Sleep/resume during
      browsing and launching. Sound pauses and returns gently at the saved setting.
- [ ] Verify gameplay picture, sound, controls/releases and stop/restart for a
      representative session. Record duration and maintainer verdict explicitly.
- [ ] Record measured frame/input/stop timing and any package/resource failures.

Do not mark the candidate as an accepted stream build on host-test results alone.
