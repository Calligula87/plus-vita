# Input worker 1: first Vita result

2026-10-15. C, now accepted: `live-720p-refs5-input-worker-1`.
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
4ms polling cadence on Vita while the existing video path continues. The
maintainer then reported **"I felt the response better. I think we improved"**
and confirmed **"Yes—all remained good, including restart"** in answer to the
picture/sound/all-controls/releases/stop/restart question. This supplies the
positive subjective comparison and no-reported-regression evidence.
No numerical physical input-to-screen measurement was made.

The planned candidate soak was 8–10 minutes; after seeing the actual duration,
the maintainer explicitly answered
**"Accept the 7m18s comparison duration"**. This satisfies the revised duration
criterion for this C comparison; it is not represented as an actual eight-minute
run. With both the human comparison and the hardware log, **accept C as the
current working input build and complete #6/#7**. This accepts the identified
package, not an untested combination or a claim of 75% less game-response delay.

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

The existing audio backlog is higher in this sample. The maintainer subsequently
confirmed sound remained as good as the baseline. No playback policy changed;
this one counter difference cannot prove C caused an audio regression, and
the pre-existing backlog still warrants the separate logging investigation.

2550 synchronous diagnostic writes account for approximately 26.982s (~6.16%
of elapsed time), using the rounded mean. This is aggregate serialized file
work, not CPU usage or continuous freezing. C deliberately omits per-control
transition logs and counts/suppresses sender-thread diagnostics rather than
persisting them while holding input locks. That documented logging difference
and network variation limit attribution of any gameplay improvement.

The current-submission presentation intervals are not output-picture age or
physical input-to-screen latency. No recording-derived measurement is available.

## Accepted package and next experiment

Accepted working build: C, `ux0:data/plus-vita-input-worker-1.vpk`, SHA256
`95437199D32328FB38197C197DCC2E9B91633F47E0D71B0E052CD0BED9FD821B`.
Retain original B (`B48C54FD…`) for the isolated logging comparison and original
accepted S (`6F4FEB9A…`) as the older fallback, as well as the full hashes in the
package manifest. C itself remains available to restore after testing L.
Neither source defaults nor executable contents change as part of recording
this acceptance; rebuilding C requires input ON and deferred logging OFF.

**Next: #10, separate B-versus-L**, `ux0:data/plus-vita-deferred-log-1.vpk`,
build `live-720p-refs5-deferred-log-1`, SHA256
`06610CC8C42A09104F118D7B79CF62BE97638A0EB61D6E5D391F1BF219787AB7`.
Its staged file was re-downloaded and hash-verified. L retains B's input owner,
so compare its logging/stream behavior with original B; it does not contain
the now accepted input-worker improvement. Test the same scene, response
actions, controls/releases, picture/sound, clean stop/restart and 8–10 minutes
from the first live picture, saving its log before another allocation.

Keep the parent improvement task open. Do not combine or accept input+deferred
logging until L has its own evidence and a subsequent combined decision/test.
