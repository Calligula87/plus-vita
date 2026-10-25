# Recovery and diagnostic software result (#19, #20)

2026-10-25. Software implementation is ready for the separate Vita comparison.
The [identified candidate and hardware procedure](RECOVERY_DIAGNOSTICS_TEST.md)
are experimental. Neither ticket's hardware acceptance, the #22 publication
privacy gate nor the #23 final release validation is claimed complete.

Runtime changes are in `7831a0a`, `6e04dde` and `f441b41`, based on stable commit
`a2fce19`. The original graphical and stream baseline artifacts are unchanged.

## Implemented behavior

- Fixed recovery messages distinguish confirmed credential rejection,
  connectivity, service and unsupported-title failures. Only the confirmed
  preflight HTTP 401 category invalidates sign-in; HTTP 403/429/503 and transport
  failures preserve the credential for an explicit retry.
- Power/Wi-Fi monitoring retires affected work through the accepted input-neutral,
  worker/media and transport shutdown path. Wake does not automatically reuse or
  allocate a session. Late operation results cannot start cancelled work.
  Offline local setup remains usable.
- A shared production attempt boundary replaces diagnostics before queued Play
  or account connection Retry can fail offline, after any previous network
  workers relinquish ownership. Completed attempts drain/close and remain on
  disk through catalogue navigation.
- The deferred sink discards all library text, permits exact known events and
  typed aggregate fields, resets sequence/counters and bounds the entire file to
  1 MiB. Critical final records have reserved space, with separate actual-write,
  quota-loss and tail-retention counts. Open/header I/O is also deferred; failed
  or stuck storage reports unavailable without raw fallback errors or unsafe
  writer reuse. Settings uses the real persistence status.

The native schema and ownership contract are in
[NATIVE_DIAGNOSTICS.md](NATIVE_DIAGNOSTICS.md). Desktop diagnostics remain
memory-only. Sign-in JSON, credentials and actual device logs were not published.

## Validation

The complete native host suite passed, including account/storage and UI workflow,
catalogue/artwork, font/raster/layout, recovery, bounded deferred logging, timing,
independent input and its combined deferred variant, audio queue, feedback wire
equivalence, FEC and reorder checks. All 34 desktop reference tests passed.
Graphical and historical diagnostic Vita builds passed.

Behavioral regressions exercised the actual allocation worker and generated
authorization preflight with simulated HTTP responses and blocked completion.
The shared native workflow and real deferred file writer additionally verified
offline Play/account Retry replacement, sequence restart, fixed failure evidence,
stale-effect rejection and preservation of an earlier worker's ownership.
Persistent-boundary checks used only synthetic canaries and covered unknown and
oversized inputs, whole-file quota, final-record retention, actual persisted
record counts, queue overflow, failed thread creation, unavailable/full storage,
blocked startup/writes, reset and bounded shutdown.

The VPK inventory contains only the executable, SFO, public CA bundle, font and
dependency license resources. This inventory check does not replace #22's
source/history/executable/public-material privacy audit.

Reproduction with the pinned dependencies and VitaSDK available:

```sh
cmake -S vita-client -B build/vita-recovery-19 -DCMAKE_BUILD_TYPE=Release \
  -DPLUS_VITA_DIAGNOSTIC_APP=OFF -DPLUS_VITA_FONT_COMPARISON=OFF \
  -DPLUS_VITA_FONT_DEFAULT_FREETYPE=ON -DPLUS_VITA_INPUT_WORKER=ON \
  -DPLUS_VITA_DEFERRED_LOG=ON -DPLUS_VITA_PUBLIC_APOLLO_TEST=OFF \
  -DPLUS_VITA_TLS_PROBE_URL= -DPLUS_VITA_TLS_TRUSTED_CA_FILE= \
  -DPLUS_VITA_PUBLIC_HTTPS_URL= -DPLUS_VITA_POST_ECHO_URL=
cmake --build build/vita-recovery-19 --parallel
```

Run each `tools/test-*.sh` host script, plus
`PLUS_VITA_INPUT_TEST_DEFERRED_LOG=1 sh tools/test-input-worker.sh`.
When the host lacks CMake, set
`PLUS_VITA_CLOUD_SESSION_TEST_SOURCE=build/vita-recovery-19/generated/cloudsession_vita.c`
for the recovery test to compile the exact generated preflight from the Vita
build. The normal host test generates it using the same CMake patch module.

## Standards

Independent review against `a2fce19` through `f441b41`: no documented-standard
violations or remaining actionable smells. The initial duplicate-reset finding
was resolved by the shared attempt boundary needed for the offline failure fix.
Accepted control/media policy and baseline preservation were retained.

## Spec

Independent final review: no remaining specification findings. Review identified
offline setup interruption, inaccurate persistence accounting and offline
Play/Retry resetting too late. Production fixes and behavioral regressions
addressed all three. Hardware recovery/playback and publication checks remain
explicitly pending.

Final totals: Standards 0; Spec 0. The next required evidence is the Vita test
result for this exact candidate. #22 is still the publication gate; this build
must not be described as the public beta or hardware accepted.
