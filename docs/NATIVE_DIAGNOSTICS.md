# Native persisted diagnostics

This contract applies to the native Vita app. The desktop reference recorder
remains memory-only under [diagnostics-policy.md](diagnostics-policy.md).

The app starts one diagnostic attempt on launch and replaces it before an
accepted explicit Play or fresh connection retry, including failed allocation.
A busy request while an earlier allocation is closing cannot start a new
attempt. That earlier worker must finish before a new one can own the log.
There is no archive or automatic upload. The single path is
`ux0:data/plus-vita-allocation.log`.

The current development identity is `recovery-diagnostics-1`, with the accepted
720p independent-input/deferred-writer profile. This is an experimental
candidate; [the stable graphical baseline](STABLE_GRAPHICAL_BASELINE.md) remains
the hardware comparison. It is not the public beta or a passed #22 gate.

## Persisted schema

Only locally generated records reach the writer:

- Fixed public build/profile constants, a local sequence and relative elapsed
  milliseconds. Sequence and elapsed origin restart for each attempt.
- Exact known allocation, stream, recovery, account-completion and UI-stage
  event names. Unknown names are dropped. Numeric result codes are limited to
  the native error range `-1..21`; unknown codes map to `-1`. Allocation stages
  are `0..5`; UI stages are the public screen enum `0..17` in `app.h`.
- Fixed library severity categories (`library.info`, `library.warning`,
  `library.error`). The library callback discards its entire text parameter,
  including purported summaries, addresses and remote disconnect reasons.
  Dynamic progress maps to `working`; the Play adapter supplies fixed stage
  numbers independently.
- Exact approved metric/counter names from `diagnostic_log.c`, aggregate
  durations/counts, UI draw/present/sample-to-present/poll aggregates and fixed
  artwork-result counters. Unknown metric/counter names and enum values are
  dropped. These are local observations, not physical input response latency.

No account/game text, payload, URL, address, port, identifier, secret, remote
reason, absolute timestamp or arbitrary key/value field is accepted. Numeric
APIs are for their documented aggregate quantities only. New fields require
explicit schema review and persistent-boundary canary tests.

## Bounds and ownership

Each record has a 1024-byte bound. Deferred queues hold 128 ordinary and 64
critical records. Ordinary overflow drops new records; critical overflow keeps
the newest records. The whole file is at most **1 MiB**, including header,
terminal records and loss summaries. Once the body quota is exhausted, a
64-record memory reserve keeps the newest known terminal/error records for
shutdown. Final typed stream metrics and counters use the critical reserve.
Sink summaries account for queue, quota, truncation and drain losses.
`persisted` counts numbered records actually written, including the retained
tail; `tail_retained` identifies that reserve, `tail_evicted` counts its replaced
records, and `budget_dropped` counts actual quota losses (including evictions).

Completed, failed and cancelled Play attempts drain/close after their allocation
worker has finished, so terminal records are on disk while the user browses.
Return to the catalogue does not replace the file. Startup-only evidence closes
on app exit or the next explicit Play. UI timing snapshots are capped at 64 and
reset before Play.

The deferred worker owns open, header, writes, flush and close. Producers do no
file I/O. Availability remains false until initialization actually persists;
Settings updates from that status. Failed/full storage disables successful
persistence without falling back to raw error output. Shutdown/reset waits at
most two seconds for the writer. A stuck device call retains one bounded worker,
file and buffers; reset fails rather than reopening or reusing them. Playback
can proceed with diagnostics unavailable. This does not guarantee that the
device's own filesystem syscall can be interrupted.

Host checks: `tools/test-deferred-log.sh` exercises real file output with
synthetic private-data canaries, oversized/unknown inputs, whole-file bounds,
final retention, overflow, reset/sequence, blocked startup/writes, full/unavailable
storage, failed thread creation and writer ownership. `tools/test-latency.sh`
and `tools/test-input-worker.sh` retain the accepted timing/input checks.
Vita retrieval, sleep/Wi-Fi cleanup and playback comparison remain hardware
checks in [RECOVERY_DIAGNOSTICS_TEST.md](RECOVERY_DIAGNOSTICS_TEST.md).
