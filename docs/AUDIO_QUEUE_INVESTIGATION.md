# Existing audio queue residence investigation

For [#9](https://github.com/Calligula87/plus-vita/issues/9), 2026-10-14.
Production audio playback code is unchanged. Raw local logs are private.

## Observations

Verified source identities: `live-720p-refs5-video-reorder-1` in
`plus-vita-video-reorder-1-result-20261013.log`, and
`live-720p-refs5-latency-baseline-1` in
`plus-vita-latency-baseline-1-result-20261014.log`. The latter has a clean
352645ms run. The accepted log's last video elapsed report is 394324ms; its
configured 600000ms limit is not its actual duration. Exact wall duration was
not recorded there. The baseline's picture/stability was confirmed equivalent
by the maintainer, with input delay still noticeable.

| Counter | Accepted | Diagnostic baseline |
| --- | ---: | ---: |
| Decoded real 10ms units | 39465 | 35148 |
| Queue residence average / max, ms | 250.148 / 319.423 | 230.748 / 315.463 |
| Periodic reports / output-gap stalls | 40 / 41 | 36 / 37 |
| Reports per minute of decoded media | 6.08 | 6.15 |
| Gap stalls per minute of decoded media | 6.23 | 6.32 |
| Largest blocking output call, ms | 18.472 | 18.466 |
| Stale dequeues / local evictions / concealment | 118 / 13 / 6 | 132 / 2 / 31 |
| Sequence gap events / maximum missing units | 15 / 3 | 30 / 3 |
| Output failures / sequence resets | 0 / 0 | 0 / 0 |

Media normalization uses decoded units × 10ms, not a falsely exact wall clock.
The baseline wall rates are 6.13 reports and 6.30 stalls/minute. Similar rates
predate the new diagnostics. Report/stall correspondence is a hypothesis to
test, not proof that each stall was caused by that report.

Queue residence is measured from local push time to dequeue **before** stale
sequence rejection, decode or output. It includes stale dequeues, excludes
units evicted before dequeue, concealment generated without arrival and any
units abandoned at stop. Both final queue counts are zero. With no decode
failure, sampled dequeues are decoded + stale: 39583 and 35280. Using observed
maximum ages bounds the change from excluding stale samples below roughly
1.0ms and 1.2ms respectively; stale accounting cannot explain ~230–250ms.
Evicted ages remain unknown, so these averages do not describe every arrival.

Both first dequeues had age 58us and at most two units had arrived. By decoded
unit 1000, cumulative averages were already 181.472ms and 170.765ms, with queue
counts 19 and 18. A large queue accumulated *before the first dequeue* therefore
does not fit these runs. Early stalls after that first sample, receive bursts,
output/consumer pacing and shared synchronous logging remain candidates.
The data does not locate their exact contribution inside those first seconds.

The queue holds 32 × 10ms units (~320ms of encoded media). A worker that outputs
at approximately the incoming rate can retain backlog without new loss. The
960-sample output block is 20ms; each real or concealed unit is 480 samples.
Up to three missing wire units can insert 30ms of concealment per sequence gap;
local evictions are skipped rather than concealed. These operations affect
consumer work, but the existing cumulative reports cannot assign a unique
share of the backlog to them, network bursts or local scheduling.

## Reproduction

Run `sh tools/test-audio-queue.sh` in a POSIX host compiler environment. It
compiles the **unchanged** `audio_playback_vita.c`, exercising actual push,
bounded queue, worker, sequence handling, PCM accumulation and output calls.
Only external Opus, Vita device/clock and diagnostic writer boundaries are
synthetic: silent 480-sample codec units, controlled arrival times, output
pacing and writer delays. It is not a codec quality test or a Vita scheduler
emulator. Each scenario runs about 100s of virtual output; sanitizer scheduling
can vary the few final drained units and final rounded averages.

| Controlled scenario | Observed prediction/result |
| --- | --- |
| No backlog, equal input/output rate | About 5ms mean queue age; no stalls/drops. |
| 200ms arrivals accumulated before consumer starts | About 185ms mean persists without writer delay; does not fit the observed first dequeues. |
| 200ms stall after first decoded unit, before output starts | About 205ms mean with initially low dequeue age; early shared logging/scheduling remains plausible, not established. |
| Startup backlog + two 10ms writes every 1000 units, deadline-paced output | Periodic gap spikes, peak about 220ms; mean about 205ms (the first pair precedes output startup). |
| Same writer delay, fixed relative 20ms output wait | Mean around 275ms and capacity pressure/drops: pacing changes the lasting effect of a stall. |
| Startup backlog + output period 20us longer than 20ms | Mean about 235ms, growing tail without periodic stalls; demonstrates a possible mismatch, not evidence of hardware clock drift. |

Address/undefined and thread sanitizer runs cover these scenarios. Both
startup accumulation and pacing can produce large averages; rising cumulative
averages alone cannot distinguish them. The controlled 20us mismatch is an
illustration, not an estimate obtained from hardware.

The measured whole-sink write mean is 10.565ms, so two isolated typical writes
would be about 21ms per periodic report. Their real individual costs and shared
mutex waits were not recorded per audio event. It is unjustified to multiply
that mean into an audio-only total or claim it explains the entire backlog.
Writer stalls can establish persistent backlog when output cannot catch up;
deadline pacing can instead absorb them. Native output buffering/clock behavior
and the first seconds' shared scheduling remain hardware-only gaps.

## Controlled next experiment

Compare the original diagnostic baseline with **L only**, the deferred logging
candidate, using the same scene and network for 8–10 minutes. Keep audio queue,
concealment, 48kHz stereo/mono contract, 960-sample output block and unity volume
unchanged. Retrieve logs before the next session. Compare unit-1/unit-1000 and
later cumulative ages, arrival gaps, empty waits, gap/call stalls, stale/drop/
concealment counts normalized by duration, writer costs and sink losses.
Check sound quality, dropouts, clean stop/restart and video/control regressions.

Prediction: if shared diagnostic I/O dominates early backlog or periodic
spikes, L reduces those ages/stalls with zero persistence loss. If it only
reduces periodic spikes while a backlog remains, investigate timestamped
arrival/dequeue/output windows in a separate diagnostic task before tuning any
playback setting. If neither changes, output/network scheduling needs closer
measurement. No numerical input-to-screen latency can be inferred from audio
queue residence; the audio worker does not gate video presentation.
