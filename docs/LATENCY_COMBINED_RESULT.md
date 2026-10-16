# Accepted combined input and logging result

2026-10-15. Hardware gate [#12](https://github.com/Calligula87/plus-vita/issues/12)
passes. The maintainer reports **"Response at least as good; everything worked"**
against accepted worker-1 C, including picture, sound, controls/releases and
stop/restart. They explicitly accepted the recorded **6m59.375s** live comparison
duration. This revises the original 8–10 minute gate for this run only.

Accept D as the current working package. The scoped input/logging phase in
[#1](https://github.com/Calligula87/plus-vita/issues/1) is complete: C improved
perceived response, isolated L validated deferred diagnostics, and D retains
both without a reported regression. Numerical physical input response latency
has not been measured, and this does not establish an optimal configuration.

## Identity and evidence

- Package: `ux0:data/plus-vita-input-worker-deferred-log-1.vpk`, 1678933 bytes.
- SHA256: `9A05307CB0E7DDBE2BC80D420A694A95B874E03CD8CEDE837E1C5884D0C83FC8`.
- Installed log build: `live-720p-refs5-input-worker-deferred-log-1`;
  timing mode: `input-worker+deferred-log`; implementation commit: `5ff453b`.
- Preserved private log: `build/debug/plus-vita-combined-1-result-20261015-233231.log`,
  271774 bytes, SHA256
  `E18BC1297A46D5BB7E09271CC97863B1E6AD2A6A94982D8F232C06C1423DDC72`.
- Private aggregate analysis: `build/debug/plus-vita-combined-1-analysis-20261015.json`.
  All 21 metrics are present, with zero invalid timing samples.

The log ends with a user stop, successful neutral consumption, input/feedback
joins, touch restoration, disconnect, audio drain/release and session return.
The maintainer's restart confirmation complements these local outcomes; local
neutral consumption does not establish remote acknowledgement.

## Comparison

Means and maxima are local measurements. Counts/rates cover each recorded live
run, not identical reception conditions. B/C/L results and their individual
duration decisions are linked below.

| Measurement | B: baseline | C: worker-1 | L: logger only | D: combined |
| --- | ---: | ---: | ---: | ---: |
| Accepted live duration | 7m46.418s | 7m18.286s | 5m02.205s | 6m59.375s |
| Input poll gap mean / maximum, ms | 15.987 / 102.778 | 4.000 / 7.196 | 15.691 / 24.867 | 4.000 / 7.031 |
| Video queue residence mean / maximum, ms | 15.337 / 89.337 | 12.553 / 110.034 | 14.381 / 42.516 | 13.905 / 39.128 |
| Main mutex wait mean / maximum, ms | 0.164 / 69.403 | — | 0.026 / 2.102 | 0.032 / 2.259 |
| Presented updates per second | 54.20 | 54.15 | 58.13 | 58.63 |
| Skipped native outputs | 8.91% | 9.06% | 2.42% | 1.42% |
| FEC repairs per minute | 48.88 | 55.03 | 10.92 | 5.15 |
| Recovery episodes | 3 | 4 | 1 | 0 |
| Audio queue residence mean / maximum, ms | 246.675 / 315.514 | 269.273 / 326.462 | 189.701 / 226.531 | 71.222 / 263.597 |
| Audio output gap stalls / minute | 6.17 | 6.16 | 0.79 | 3.58 |

The dashes omit a comparison value rather than implying zero. Presentation
counts are updates, not proof of unique game frames or physical scanout rate.
D needed far fewer FEC repairs than C or B. Consequently its higher presentation
rate, fewer skips and lack of recovery pauses cannot all be attributed to the
code changes. The important retained input result is the independent 4ms mean
cadence and the maintainer's response/quality assessment.

## Input and diagnostics

D made 104694 polls, with one missed deadline, zero polling/send failures,
6597 published and consumed updates, and zero coalesced updates. Native
timestamps changed 25102 times and repeated 79592 times: a 4ms peek is not a new
native sample every 4ms. Mean sample-to-state-send was 0.320ms (maximum 1.192ms);
sample-to-history-send was 0.409ms (maximum 0.778ms). These start at a local
sample, not at the physical press, and do not include cloud processing.

All **2381** diagnostic records persisted in consecutive sequence order.
Ordinary drops, critical drops, truncation and shutdown drops were all zero;
the sink reported `io_ok=1`. Mean producer enqueue was **27.65us**, maximum
169us. Actual writer persistence still averaged **10.592ms**, maximum 69.310ms,
totalling 25.219s across the allocation. Enqueue excludes upstream formatting
and filtering; whole-allocation sink totals and preview-only B timings have
different scopes. Empty producer `log_write_call` means no producer file work,
not free storage. The final sink line precedes final flush/close, so the human
result/lifecycle confirmation remains part of acceptance.

Video queue peak was 5/16, with zero incomplete access units, zero FEC failures
and zero recovery episodes. Mean decode was 6.818ms and display wait 9.563ms.
Mean first-packet-submission-to-present was 39.250ms; this is a local submission
interval. Native output lacks a correlated per-picture timestamp, so neither
this span nor its 284.144ms maximum measures picture age or input response.

## Remaining audio uncertainty

Sound was accepted, with zero failed audio outputs, zero output-call stalls,
zero dropped outputs and zero sequence resets. The whole-run queue mean fell
to 71.222ms, but a late arrival gap reached 267.594ms and queue residence reached
263.597ms. The final 853 dequeued units averaged approximately **245ms**, derived
from rounded cumulative totals after the 41000-decoded-unit report. This is an
approximation, not an individual-sample distribution. Eighteen units still
queued at stop are excluded from the residence mean.

There were 25 output-gap stalls (3.58/min), versus L's four (0.79/min).
The result does not establish a permanent 71ms backlog, a fixed audio pipeline,
or a proven clock-drift cause. Audio policy was unchanged; further audio tuning
would be a separate experiment under the existing investigation.

## Adoption and reproducibility

Keep the tested D installed; no replacement upload or retest is needed for
this accepted phase. Retain accepted C as the immediate fallback, SHA256
`95437199D32328FB38197C197DCC2E9B91633F47E0D71B0E052CD0BED9FD821B`,
and preserve original B/L/S. The accepted bytes are unchanged by this decision.
Reproduce D with a Release Vita configuration and both
`PLUS_VITA_INPUT_WORKER=ON` and `PLUS_VITA_DEFERRED_LOG=ON`; source defaults
remain OFF and isolated profiles remain available. The final software suite,
native build, profile identities and two-axis review passed before the test;
this update changes evidence and documentation only.

Related: [B baseline](LATENCY_BASELINE_FOLLOWUP_RESULT.md),
[C acceptance](LATENCY_INPUT_WORKER_RESULT.md),
[L validation](LATENCY_DEFERRED_LOG_RESULT.md),
[combined software/procedure](LATENCY_COMBINED_TEST.md),
[audio investigation](AUDIO_QUEUE_INVESTIGATION.md), and
[ADR 0001](adr/0001-preserve-stream-baseline-during-input-latency-work.md).
