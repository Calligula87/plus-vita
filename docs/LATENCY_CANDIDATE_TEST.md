# Isolated latency candidates

Software preparation for [#5](https://github.com/Calligula87/plus-vita/issues/5)
and [#8](https://github.com/Calligula87/plus-vita/issues/8), 2026-10-14.
Neither candidate is accepted on hardware yet. Keep the original diagnostic
baseline and accepted reorder VPKs for comparisons and rollback.

Staging status, 2026-10-15: C, L, original B and accepted rollback S are uploaded
to `ux0:data/` under the filenames below. All four SHA256 hashes match downloaded
readbacks. The existing `plus_vita_smoke.vpk` remains the original diagnostic
baseline and was backed up/verified; the existing result log was preserved.
No uploaded package has been installed or accepted automatically.

## Packages and order

All packages have application ID `PLVT00001`: installing one replaces that app
version. Staging a file over FTP does not install it. The package manifest in
`build/debug/latency-candidates-1-manifest.json` records SHA256 and readback
verification. Logs are overwritten by the next allocation; retrieve each log
before starting another session. Keep credentials and raw logs private.

| Role | Filename in `ux0:data/` | Build on result screen/log |
| --- | --- | --- |
| B: original diagnostic baseline | `plus-vita-latency-baseline-1.vpk` | `live-720p-refs5-latency-baseline-1` |
| C: independent input | `plus-vita-input-worker-1.vpk` | `live-720p-refs5-input-worker-1` |
| L: deferred logging, for a later separate comparison | `plus-vita-deferred-log-1.vpk` | `live-720p-refs5-deferred-log-1` |
| S: accepted rollback | `plus-vita-video-reorder-1.vpk` | `live-720p-refs5-video-reorder-1` |

1. B's [baseline checks](LATENCY_BASELINE_FOLLOWUP_RESULT.md) are complete:
   the maintainer accepted 7m46.418s and subsequently confirmed all remaining
   requested checks done; a distinct 5m12.106s baseline allocation ended cleanly.
   Logs are preserved. No further baseline-only run is required before C.
2. **Next: install C** and compare B and C in the same game/scene and Wi-Fi
   conditions: repeat the same ten button presses or short stick movements.
   Optional 120/240fps phone video can estimate button-to-screen response;
   otherwise report a subjective better/same/worse result. Run C for 8–10
   minutes and test held-button/stick release, front/rear touch, individual
   START/SELECT, the stop chord and restart. Stop early for stuck controls,
   missing controls, frozen video or worsening sound.
   The [first C hardware result](LATENCY_INPUT_WORKER_RESULT.md) now verifies
   4ms mean/7.196ms maximum polling with clean stop. The maintainer explicitly
   accepted its 7m18.286s comparison duration; human response/quality/control
   and restart observations remain pending. C is not automatically adopted.
3. Retrieve `ux0:data/plus-vita-allocation.log` after each run and report build,
   duration, responsiveness and any regression. Tell the agent when a log is
   ready; it can retrieve it over the authorized FTP connection.
4. Compare B and L separately after the input comparison. L retains baseline
   input sampling. Check sink drop/truncation/I/O counters, stop completion,
   sound continuity and repeated sessions. Do not combine C and L for the first
   comparisons. Follow [the full test plan](LATENCY_TEST_PLAN.md) for acceptance.

## What changes in C

One input owner peeks pad/front/rear touch every requested 4ms, preserves the
existing mappings and touch identities, deduplicates controller states and
publishes through the existing feedback sender. Video/decode/display no longer
owns input sampling. Missed deadlines skip ahead without catch-up spinning.
The requested cadence is not a guarantee of native sample freshness: repeated
native timestamps are counted, and touch sampling still runs on those ticks.

START+SELECT remains suppressed as a combined gesture and stops after two
seconds. Shutdown requests idle, checks local consumption for up to 100ms,
then joins before the feedback sender/transport or touch setup is retired.
That check is not a server acknowledgement. Fatal device and observed send
errors stop the candidate and appear in its result. The existing sender's
locking and UDP calls retain their platform behavior.

**Comparison limitation:** C omits the old per-control transition log lines
and emits aggregate input counters at shutdown, avoiding disk I/O in its input
thread. The baseline had about 285 such writes. C also suppresses diagnostics
from its feedback sender thread, including nested transport diagnostics, and
reports their count after join as `suppressed_logs`; local send-failure counters
are retained. This prevents a failure log from holding the sender's state
mutex across disk I/O. Other threads keep synchronous diagnostics. Any gain
could include reduced logging as well as input isolation; do not attribute it
entirely to scheduling.

## What changes in L

L keeps the baseline input owner and moves diagnostics to a separate writer.
Producers filter/redact first and copy a bounded record while holding a short
queue mutex. The writer releases that mutex before all persistence calls.
There is no synchronous fallback on a streaming producer.

Storage is 128 ordinary and 32 reserved critical records, each at most 1024
bytes, plus one in-flight record and a 128KiB writer stack. Ordinary overflow
drops new records; critical overflow evicts its oldest record to retain recent
errors and final summaries. Retained records drain by enqueue sequence, not
wall-clock or cross-thread event order. Counters report ordinary/critical
drops, truncation, shutdown drops, persisted records and I/O failure.

End stops acceptance and allows at most two seconds of waiting for drain. A
stuck file call is not forcibly cancelled: its single bounded worker retains
the file/queue, persistence is marked failed, and reopening is refused while
it remains unfinished. Later begin/end can reap it after it finishes. The
result screen reports unavailable diagnostics on I/O/shutdown failure. If the
filesystem remains unusable, return to the accepted build and report the
result screen; a failed file cannot reliably contain its own final error.

The final `sink deferred` line distinguishes **producer enqueue** and **writer
persistence** count/total/max microseconds. In L, legacy `log_write_call` is
empty because the producer does no file work; this is not evidence of zero
disk cost. Queue contention remains `log_mutex_wait`. Sink totals include the
whole allocation session, so compare their scope carefully with preview-only
baseline metrics. Failure of the final summary/close itself is reported by
the result screen even if an earlier `io_ok=1` was written to the file.

Input/video/protocol/FEC/reorder/display/audio policy and mappings stay at the
baseline. Successful host tests and packaging do not prove a response gain or
hardware stability; tasks #3, #6, #7 and #10 remain the human/acceptance gates.

Host coverage includes the real production decoder stalled at `sceAvcdecDecode`
while input reaches the real sender and its terminal stop/neutral completes.
A separate fixture blocks the real synchronous diagnostic writer, injects a
transport error that logs through the real sink, and checks sender/input join
without releasing disk first. The full Vita preview/native transport and touch
restoration still need the hardware stop/disconnect/restart checks.
