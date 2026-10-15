# Input worker 1: first Vita result

2026-10-15. Candidate C: `live-720p-refs5-input-worker-1`.
Related [hardware comparison #6](https://github.com/Calligula87/plus-vita/issues/6),
[adoption/rollback #7](https://github.com/Calligula87/plus-vita/issues/7),
and [baseline evidence](LATENCY_BASELINE_FOLLOWUP_RESULT.md).

## Evidence and verdict so far

Retrieved the user-authorized log into private ignored artifact
`build/debug/plus-vita-input-worker-1-result-20261015-225014.log`, 326691 bytes,
SHA256 `13FEF069DF6AC9405113CCC9E617D5D6DCF898237FDFAFB87F7E6FF6B50FC9BB`.
Structured aggregates: `build/debug/plus-vita-input-worker-1-analysis-20261015.json`.
The installed live build and `mode=input-worker` are verified. The staged VPK
was separately readback-verified against SHA256
`95437199D32328FB38197C197DCC2E9B91633F47E0D71B0E052CD0BED9FD821B` before
this test; a runtime build tag does not itself hash the installed binary.

The run lasted **438286ms (7m18.286s)** before clean user stop. Native restore,
disconnect, stream completion, allocation completion and audio drain/release
succeeded. All 21 duration metrics are present, invalid spans zero. Input poll
and observed send failures were zero. The neutral request succeeded and local
consumption was confirmed before sender teardown; this is not remote receipt.

**Established:** the separate input owner delivers a much steadier requested
4ms polling cadence on Vita while the existing video path continues.
**Not established yet:** repeatable physical/subjective response improvement,
same-scene control/audio/quality equivalence or subsequent-session success.
The user was asked for those observations. The planned candidate soak is 8–10
minutes; this run is shorter. The earlier explicit duration exception applied
to baseline B only, so it is not silently extended to C. Keep #6/#7 open and
do not promote C from local metrics alone.

## Input timing

Milliseconds. B is the accepted 7m46.418s diagnostic baseline session. Percentile
entries are conservative histogram bucket upper bounds, not exact percentiles.

| Metric | B mean / max | C mean / max | C p95 upper |
| --- | ---: | ---: | ---: |
| Input poll interval | 15.987 / 102.778 | **4.000 / 7.196** | 7.196 |
| Changed-state publication call | 0.025 / 0.099 | 0.024 / 0.122 | 0.122 |
| Poll start to publication | 0.071 / 0.162 | 0.066 / 0.283 | 0.250 |
| Publication to sender consumption | 0.073 / 0.824 | 0.062 / 0.399 | 0.250 |
| Poll start to local state-send return | 0.368 / 1.833 | 0.319 / 1.058 | 0.500 |
| Poll start to local history-send return | 0.398 / 0.908 | 0.355 / 0.972 | 0.972 |

Mean polling interval fell about **75%** (roughly four times as frequent).
Earlier baseline sessions averaged 15.163–15.987ms with 57.513–102.778ms maxima;
the new maximum is below all three. This is not a 75% reduction in whole input
response latency, nor an assertion that each physical press saved 12ms.
The baseline already sent sampled input quickly; unobserved cloud/game/video
processing and native hardware sample freshness still contribute.

There were 109353 polls, one missed scheduling deadline, 4001 changed-state
updates/publications/consumptions and zero coalescing. Native timestamps changed
26220 times (approximately 60/s) and repeated 83133 times (~76% of polls).
Thus the worker checks the existing provider more frequently; it does not
establish 250 distinct native samples/s or change the accepted sampling cycle.
Repeated timestamps still permit touch polling and do not flood the sender.
Suppressed sender-thread diagnostic count was zero in this run.

## Video, logging and audio

| Metric | B | C |
| --- | ---: | ---: |
| Presented updates per second, approximate | 54.20 | 54.15 |
| Compressed queue residence mean / max, ms | 15.337 / 89.337 | 12.553 / 110.034 |
| Queue peak / capacity | 6 / 16 | 10 / 16 |
| Hardware decode mean / max, ms | 6.899 / 17.124 | 6.917 / 19.230 |
| Framebuffer wait mean, ms | 9.231 | 9.161 |
| Synchronous write/flush mean / max, ms | 10.683 / 70.519 | 10.581 / 32.085 |
| Successful FEC repairs / minute | 48.88 | 55.03 |
| Recovery episodes | 3 | 4 |
| Audio queue residence mean / max, ms | 246.675 / 315.514 | 269.273 / 326.462 |

C submitted 26104 access units, produced 26097 native outputs and presented
23733 updates. 2364 decoded updates were replaced (~9.06% of outputs), versus
~8.91% in B. No compressed queue overflow, decoder/display failure or audio
output failure was found. Seven fewer outputs than submissions does not prove
an exact native picture-buffer depth or displayed-picture age.

Four recoveries resumed after 184, 111, 152 and 170ms, with 36 dependent units
discarded. Five incomplete frames, zero wholly missing-frame retirements,
zero reorder pressure drops, 402 FEC repairs and zero FEC failures were
reported. Audio arrival-gap maximum was 204.480ms, with 52 sequence gaps,
41 concealments, 18 local evictions and zero sequence resets. Audio had 45
output-gap stalls and zero output-call stalls. Reception conditions differ
from the baseline, and earlier B control-check sessions had even longer
recoveries. The lower queue mean or different losses cannot be assigned solely
to the input change; queue and decode maxima did not uniformly improve.

The existing audio backlog is higher in this sample and needs the human sound
comparison. No playback policy changed, so this one observation cannot prove
that C caused an audio regression or that sound quality is equivalent.

2550 synchronous diagnostic writes account for approximately 26.982s (~6.16%
of elapsed time), using the rounded mean. This is aggregate serialized file
work, not CPU usage or continuous freezing. C deliberately omits per-control
transition logs and counts/suppresses sender-thread diagnostics rather than
persisting them while holding input locks. That documented logging difference
and network variation limit attribution of any gameplay improvement.

The current-submission presentation intervals are not output-picture age or
physical input-to-screen latency. No recording-derived measurement is available.

## Next evidence

Record the user's repeated better/same/worse comparison and any picture,
sound, touch/control/release or restart issue. If ambiguous, repeat a short
matched B/C comparison in reverse order. The separate duration/stability gate
still needs an 8–10 minute C run or an explicit maintainer revision; do not
inherit B's exception. Retain the original rollback and all private logs.
L remains the later **separate B-versus-L** experiment, never a combined first
input-plus-logging candidate. No executable/settings change is needed to
analyze these results, and C is not accepted automatically.
