# Deferred logging 1: Vita result

2026-10-15. L: `live-720p-refs5-deferred-log-1`, compared with original B.
Related [logging comparison #10](https://github.com/Calligula87/plus-vita/issues/10).

Retrieved private log `build/debug/plus-vita-deferred-log-1-result-20261015-231149.log`,
225049 bytes, SHA256
`D254011FD97453AA84E18E26A26A86943F2D24C3D5F4C58F2EA8BB670A22E275`.
Structured analysis: `build/debug/plus-vita-deferred-log-1-analysis-20261015.json`.
Correct live build/mode, clean user stop after **302205ms (5m02.205s)**, successful
restore/disconnect/stream completion/audio drain/release. All 21 metrics are
present, invalid spans/poll failures/observed sends failures zero.

The maintainer reports **"Everything worked; little noticeable difference"**
for picture, sound, controls/releases, stop/restart and perceived response/stutter,
and explicitly accepts **"the 5m02s comparison duration"**. Record L as a
validated logging component with no reported regression, **not** a separate
user-perceived responsiveness improvement. C remains the accepted working
input build. Neither a combined build nor exact physical response is validated
by these isolated tests. The duration exception applies to this L comparison.

## Diagnostics and producer cost

All **2121** queued records persisted; sequence 1–2121 is complete and ordered.
Ordinary/critical/drop-on-drain counts and truncations are all zero; `io_ok=1`.
The final timing/audio/event summaries are present. The final sink line is
written before its final flush/close; the user's clean-result/restart report
also supplies lifecycle evidence rather than treating that line as independent
proof of every later filesystem operation.

| Measurement | L observed | Scope |
| --- | ---: | --- |
| Measured enqueue mean / maximum | **26.55 / 102us** | Formatting/queue interval across allocation; upstream log formatting/filtering excluded. |
| Writer persistence mean / maximum | 10.988 / 70.376ms | Separate writer's actual write/flush, across allocation. |
| Total producer enqueue / writer persistence | 0.0563 / 23.306s | Different, overlapping work; not continuous freezes or CPU utilization. |
| Preview queue-lock wait mean / maximum | 1 / 36us | 1777 preview producer records. |
| Legacy producer write-call count | 0 | Correct: producers no longer perform file work, not zero disk cost. |

Original B's preview synchronous write mean/max were 10.683/70.519ms, with
31.162s aggregate over its longer session. L's disk cost remains substantial,
but accepted diagnostics reach bounded storage without waiting for that I/O.
The counts/windows differ, so do not divide totals to claim a whole-system
speedup. The measured separation plus prior blocked-writer host tests directly
supports the mechanism; broader frame/audio changes remain observational.

## Streaming comparison

Milliseconds unless stated. B is the accepted 7m46.418s original baseline.

| Metric | B | L |
| --- | ---: | ---: |
| Input polling mean / max | 15.987 / 102.778 | 15.691 / 24.867 |
| Main mutex wait mean / max | 0.164 / 69.403 | 0.026 / 2.102 |
| Callback mutex wait mean / max | 0.032 / 66.828 | 0.002 / 0.619 |
| Callback work mean / max | 0.293 / 71.648 | 0.235 / 2.466 |
| Compressed queue residence mean / max | 15.337 / 89.337 | 14.381 / 42.516 |
| Presented updates/s, approximate | 54.20 | 58.13 |
| Replaced decoded updates / native outputs | 8.91% | 2.42% |
| FEC repairs/minute | 48.88 | 10.92 |
| Recovery episodes | 3 | 1 |
| Audio queue residence mean / max | 246.675 / 315.514 | 189.701 / 226.531 |
| Audio output-gap stalls/minute | 6.17 | 0.79 |

L uses B's input owner, not C's independent sampler. Its mean polling interval
therefore remains near the video cadence rather than C's 4ms. There were 2879
published/consumed updates, zero coalescing and zero send failures.

18011 submissions, 18004 outputs, 17568 presentations, 436 replaced decoded
updates, queue peak 5/16, 55 successful FEC repairs/zero failures and one
recovered episode are recorded. Reorder pressure drops/wholly missing-frame
retirements are zero. Different loss conditions, including fewer repairs and
recoveries but a 222.865ms maximum audio-arrival gap, limit causal comparison.
Lower producer-related tails fit the change; do not attribute all improved
frame/audio counters to logging or claim equivalent traffic conditions.

Audio has zero output failures/local evictions/sequence resets, 12 concealments,
113 stale dequeues, queue peak 27 and four gap stalls. Four units remained
queued at stop and are excluded from residence averages; baseline final queue
was zero. Large audio residence remains despite the lower observed mean.
Periodic synchronous logging is supported as a contributor to gap stalls;
this run still cannot establish the entire hardware backlog's cause or a clock
drift rate. No audio setting or A/V synchronization policy changed.

Two pre-stream Senkusha timeout/error lines preceded the later successful
session. No numerical physical input-to-screen gain is established.

## Decision and next work

Complete #10: isolated L's bounded persistence and human quality/lifecycle
comparison pass for the explicitly accepted duration. Retain C for current
play, plus original B/L/S packages and private logs. Prepare a **new identified
combined input+deferred-log candidate** and test its actual joint boundaries,
Release package and safe shutdown before staging. That candidate must preserve
C's responsiveness/controls and L's persistence outcomes, with its own Vita
acceptance. It is not accepted merely because both components passed alone.
