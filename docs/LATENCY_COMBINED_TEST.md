# Combined input-worker and deferred-log test

Software task [#11](https://github.com/Calligula87/plus-vita/issues/11), hardware
gate [#12](https://github.com/Calligula87/plus-vita/issues/12). Original C is
accepted for better perceived response without reported quality/control/restart
regressions. Isolated L persisted all diagnostics without producer disk work;
the maintainer noticed little difference and reported everything worked.
See [C result](LATENCY_INPUT_WORKER_RESULT.md) and
[L result](LATENCY_DEFERRED_LOG_RESULT.md).

## Identity and scope

D package: `ux0:data/plus-vita-input-worker-deferred-log-1.vpk`.
Build: `live-720p-refs5-input-worker-deferred-log-1`.
Timing mode: `input-worker+deferred-log`.
Manifest: `build/debug/plus-vita-combined-1-manifest.json` records the final
source commit, SHA256, bytes, Release flags, tests/review and upload/readback.
Do not infer installation or acceptance from upload.

D enables both existing implementations. It retains C's requested4ms sampler,
deduplication/mappings/contact continuity, native sampling mode, neutral/join
and sender diagnostic guard, and L's bounded writer, filtering, loss counters,
ordering and two-second shutdown ownership. Video/FEC/reorder/display/audio/
quality and transport policies are unchanged. Original B/C/L configurations
remain available; default options stay OFF. The previous both-ON rejection was
replaced after isolated validation, and both-ON now has a unique experimental
identity instead of inheriting C's tag.

## Software verification

The actual input/feedback/production decoder harness now accepts
`PLUS_VITA_INPUT_TEST_DEFERRED_LOG=1`; this compiles and runs the real diagnostic
sink with the input owner rather than treating separate tests as a joint test.
Memory/undefined and thread runs exercise mappings/releases, stalled native
decode, neutral/terminal stop while decode is blocked, missing/fatal input,
partial init, send failure and stop/restart while the real writer is blocked.
The isolated harness remains its default. Separate bounded-logger and existing
latency/audio/FEC/reorder/H.264 regressions remain part of the final suite.
Native Release build and ELF tag/mode are checked before upload.

```sh
PLUS_VITA_INPUT_TEST_DEFERRED_LOG=1 sh tools/test-input-worker.sh
PLUS_VITA_INPUT_TEST_DEFERRED_LOG=1 PLUS_VITA_TEST_SANITIZERS=thread sh tools/test-input-worker.sh
```

Host verification does not establish joint Vita scheduling/quality. C remains
the accepted working fallback until D's physical test is accepted.

## Human procedure

1. Keep `plus-vita-input-worker-1.vpk` (C) available. Its accepted SHA256 is
   `95437199D32328FB38197C197DCC2E9B91633F47E0D71B0E052CD0BED9FD821B`.
   B/L and older fallback S remain preserved. Installing D replaces the same
   application ID `PLVT00001`; original package files remain available.
2. Install D through VitaShell and verify the new build tag. Compare with
   **accepted worker-1 C**, using the same game/scene, Wi-Fi position/settings
   and ten separated response actions. Report better/same/worse; optional
   high-frame-rate recording is not required.
3. Run8–10minutes from the **first live picture**, and check picture/sound,
   ordinary controls/sticks, rear L2/R2/L3/R3, front touch/click and releases.
   Stop with START+SELECT, have the agent retrieve the log, then confirm another
   stream starts/stops. Any duration revision must be explicit for D; previous
   B/C/L exceptions are not carried forward silently.
4. Report any missing/stuck input, audio/video regression, unexpected stop or
   diagnostics-unavailable result screen. Stop early on problems, preserve its
   log and restore C. Do not retry into a new allocation before preserving the
   failed log.
5. After the log is retrieved, confirm D's native poll mean/tails/freshness,
   failed/coalesced sends and neutral handoff, plus sink loss/truncation/I/O
   outcomes and producer/persistence timing. Compare normalized video/audio/
   recovery counters with reception variation and final queued audio excluded
   from residence means where appropriate.

Accept D only if C's response/quality/control/restart experience remains at
least as good and useful bounded diagnostics persist successfully. A combination
is not assumed to add the measured component gains. Local timings and native
submission intervals are not numerical physical input-to-screen latency.
Record accept/reject/inconclusive in #12 before updating the accepted build.
