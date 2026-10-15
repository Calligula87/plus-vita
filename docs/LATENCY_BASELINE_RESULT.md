# Diagnostic latency baseline: first hardware result

Date: 2026-10-14. Build: `live-720p-refs5-latency-baseline-1`.
Related [spec #1](https://github.com/Calligula87/plus-vita/issues/1),
[hardware task #3](https://github.com/Calligula87/plus-vita/issues/3),
[analysis task #4](https://github.com/Calligula87/plus-vita/issues/4),
[test plan](LATENCY_TEST_PLAN.md), and
[ADR 0001](adr/0001-preserve-stream-baseline-during-input-latency-work.md).

## Evidence and gate status

Retrieved the allocation log over user-authorized FTP to ignored local artifact
`build/debug/plus-vita-latency-baseline-1-result-20261014.log`: 298158 bytes,
SHA256 `133A28F7B1853699EBDBE7C6B22329F94974D447B100FFB236FFA4A6B2E616D6`.
Only aggregate findings are reproduced here. The correct live build tag and all
21 duration metrics are present, with zero rejected backward spans.

The run lasted 352645ms (5m52.645s) before the user's stop. Preview result=0,
session err=0, disconnect send=0, clean socket disconnect, and audio
drain/release=0 establish clean termination. No queue-overflow stop,
decoder/reference guard rejection, display failure, input-send failure or audio
output failure was found.

This is useful measurement evidence, not full diagnostic acceptance. The run
is shorter than the planned 8–10 minutes, and the log cannot establish perceived
picture/audio/control quality or a matched scene comparison. Those observations
remain unconfirmed at the time of this report. No phone measurement or exact
input response latency is available. The accepted stream build is unchanged;
no input worker or new package was deployed during this inspection.

## Input and local timing

Values below are milliseconds. The percentile entries are conservative
histogram **upper bounds**, not exact percentiles. Different rows overlap;
do not add them to estimate input response latency.

| Local measurement | Mean | p95 upper | Maximum | Interpretation |
| --- | ---: | ---: | ---: | --- |
| Input poll gap | 15.812 | 25 | 57.513 | Physical changes can wait until the next poll; this is the sample interval, not the measured waiting time of each press. |
| Changed-input setter call | 0.026 | 0.069 | 0.069 | Publication itself is quick. |
| Poll start to publication | 0.071 | 0.207 | 0.207 | Includes mapping/touch sampling and sender-state locking. |
| Publication to sender consumption | 0.072 | 0.250 | 0.798 | The sender is woken promptly on changed input. |
| Poll start to state-send return | 0.357 | 1 | 1.065 | Successful local stick/state sends; not server receipt. |
| Poll start to history-send return | 0.427 | 0.815 | 0.815 | Successful local button/trigger/touch sends; not server receipt. |
| Compressed queue residence | 14.222 | 33 | 88.925 | Measured local wait before decode. |
| Hardware decode | 6.909 | 8 | 14.233 | Native decode call duration, not decoder picture age. |
| Framebuffer wait | 9.187 | 12 | 16.671 | Existing synchronous presentation wait. |
| Diagnostic write/flush under mutex | 10.565 | 20 | 35.083 | Existing synchronous file work in gameplay threads. |

There were 3851 changed input publications and 3851 consumed updates, no
coalesced publications, no local send failures and no failed input polls.
There were 3674 measured successful state sends and 288 history packets tied to
sampled changes. One update may produce multiple packet types/history events;
these counts should not be equated to physical button presses.

The measurements establish that sampled input is handed to the local send API
quickly. They do not prove that local scheduling dominates total input response
latency: unsampled waiting, cloud/game processing and returned video remain.
Changing the feedback sender's 200ms idle timeout is not supported as a fix.

## Synchronous diagnostics are a concrete cost

2461 measured writes averaged 10.565ms: approximately 26.000s of serialized
file-write/flush time during the 352.645s run, about 7.37% of elapsed time.
The total is derived from the rounded mean, so it is approximate. It is neither
26 seconds of continuous freeze nor 7.37% measured CPU utilization.

The logger writes and flushes under a global mutex. Main-loop input events,
receiver/FEC events and periodic audio reports can all call it synchronously.
Other measured tails were 31.640ms main probe-mutex reacquisition, 26.789ms
callback mutex acquisition and 45.661ms callback work. These intervals overlap
with logging and scheduling; this log does not assign every outlier to a cause.

The audio worker emits two reports every 1000 decoded units. There were 36
periodic audio reports and 37 output gaps over its 30ms threshold; the earlier
accepted run had 40 reports and 41 such gaps. This correspondence and the
source placement make logging a plausible contributor to those stalls, not a
proved explanation for the entire audio backlog. Existing logging behavior was
retained by design; the newly added timing summaries run after streaming.

## Video continuity and recovery

- 20998 submissions, 20991 native outputs, 18866 presented updates and 2125
  replaced already-decoded updates (10.12% of native outputs).
- Approximate rates over the preview duration: 59.52 native outputs/s and
  53.50 presented updates/s. Replaced decoded updates are not equivalent to
  lost compressed reference frames.
- Compressed queue peak=8, below capacity=16; no overflow stop.
- 446 successful FEC repairs, zero failures; mean repair approximately 0.479ms,
  maximum 5.906ms, total 0.214s.
- Two incomplete frames and two IDR recovery episodes. The episodes resumed
  after 106ms and 272ms, with 20 dependent access units discarded in total.
- 48 accepted late packets, 28 late-completed assemblies, reorder peak=3,
  zero pressure drops and zero wholly missing-frame retirements.
- Reorder blocked wait total=345.569ms, maximum=29.063ms; complete-frame
  ready-to-release hold total=142.003ms, maximum=24.717ms. Grace expiry is
  polled, so actual wait can exceed the nominal 20ms.

The previous accepted 395.669s run had 56 repairs, one recovery, queue peak=8,
about 54.02 presented updates/s and 8.68% replaced decoded updates. Repairs per
minute were about 75.88 in this run versus 8.49 previously. Conditions and
traffic differ; these are descriptive comparisons, not evidence that the
instrumentation caused more loss or that performance is equivalent.

Average current-submission first-packet-to-presentation-return was 36.554ms,
maximum 123.989ms. **This is not the displayed picture's age or input response
latency.** The native output may belong to an earlier access unit. Seven fewer
outputs than submissions, also observed in the accepted run, do not establish
an exact live decoder depth. The audited replay's buffered pictures remain a
reason to seek correlated output/physical-response measurements before making
whole-pipeline latency claims.

## Audio backlog is substantial and predates this build

The current audio queue residence averaged 230.748ms and reached 315.463ms,
with queue peak=32, two local drops, 31 concealed units and zero output failures.
The previous accepted run averaged 250.148ms and reached 319.423ms, with peak=32,
13 drops, six concealed units and zero output failures. Current average queue
residence rises from about 170.765ms at 1000 decoded units to 230.497ms at 35000;
that is cumulative-average growth, not a direct clock-drift measurement.

This is evidence of delayed sound already present in the accepted path, not a
newly established diagnostic regression. Queue residence excludes cloud transit,
decode and subsequent audio output latency. Audio has a separate worker and
does not gate video presentation in the current source, so its large backlog
must not be presented as an equivalent visual-response delay. Do not change
the accepted audio settings as part of the first input experiment.

## Ranked investigations and predictions

1. **Existing synchronous logging:** if hot-path file work contributes to
   scheduling tails, a separate bounded/deferred logging experiment should
   reduce those write stalls and related poll/queue tails while preserving
   stream/control/audio policies. Do not combine it with another performance
   change if causal comparison is the goal.
2. **Input coupled to video work:** if sampling is delayed by decode/display
   work, an independent input owner should reduce poll gaps while keeping
   publication-to-send timing and feedback behavior comparable. A 2–4ms
   requested cadence is a candidate to test, not established hardware behavior
   or a promised response-time gain.
3. **Video residence/native buffering:** queue residence is measured; native
   output age is not. A recording or correlated native-output measurement is
   needed before attributing the remaining visual response to decoder buffering.
4. **Cloud/game/network and audio:** they may add response delay outside these
   local measurements. The audio backlog deserves its own later investigation;
   this log cannot establish its entire cause or the cloud/game processing time.

At the maintainer's subsequent triage request, the interpretable results unblock
software preparation: [#5 input isolation](https://github.com/Calligula87/plus-vita/issues/5)
first, [#8 deferred logging](https://github.com/Calligula87/plus-vita/issues/8) as
a separate candidate, and [#9 audio backlog diagnosis](https://github.com/Calligula87/plus-vita/issues/9)
without playback changes. Baseline acceptance remains open in #3 and directly
blocks hardware comparisons #6 and #10; analysis completion does not waive it.
Do not reopen display-worker work, shorten reorder grace, discard compressed
references or claim a numerical end-to-end improvement from these results.

## Maintainer observation after triage

The maintainer confirmed: "Same quality and stability; input delay still
noticeable." This satisfies the subjective quality/stability observation for
the inspected run. Its duration remains 5m52.645s; the separate 8–10 minute
and restart checks are still required before candidate hardware comparisons.

## Follow-up, 2026-10-15

A second clean baseline ran for 7m46.418s with all 21 timing summaries and no
invalid spans or input-send/audio-output failures. The maintainer explicitly
accepted that duration, but reported front touch untested and rear controls
uncertain. #3 remains open for the remaining short control/release, response
action and restart checks. See the [follow-up report](LATENCY_BASELINE_FOLLOWUP_RESULT.md)
for comparison metrics and the preserved evidence hash.
