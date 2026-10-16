# Playable milestone before release

2026-10-16. The maintainer accepts the latest tested combined VPK as playable
and closes the first input/logging improvement phase. This package is the
working baseline for release preparation. It has not been published as a
public release, and acceptance does not require rebuilding the tested bytes.

## Accepted package

| Field | Value |
| --- | --- |
| VPK | `plus-vita-input-worker-deferred-log-1.vpk` |
| Build | `live-720p-refs5-input-worker-deferred-log-1` |
| Timing mode | `input-worker+deferred-log` |
| Application ID | `PLVT00001` |
| Size | 1678933 bytes |
| SHA256 | `9A05307CB0E7DDBE2BC80D420A694A95B874E03CD8CEDE837E1C5884D0C83FC8` |
| Runtime implementation commit | `5ff453b` |
| Hardware acceptance documentation commit | `ef62f55` |
| Verified local artifact | `build/debug/plus-vita-input-worker-deferred-log-1.vpk` |
| Verified Vita staging path | `ux0:data/plus-vita-input-worker-deferred-log-1.vpk` |

The artifact and its downloaded FTP readback matched before hardware testing;
the retained local artifact was checked again when recording this milestone.
Build products remain outside source control. Keep the tested installed build.

The immediate fallback is accepted worker-1 C,
`plus-vita-input-worker-1.vpk`, SHA256
`95437199D32328FB38197C197DCC2E9B91633F47E0D71B0E052CD0BED9FD821B`.
Original baseline B, isolated logger L and older accepted S remain preserved.
All these packages share the application ID; installing one replaces that app.

## Acceptance evidence and limits

C improved perceived response over B. The combined D build retained response
at least as good as C, and the maintainer confirmed picture/sound, all
controls/releases and stop/restart worked well. They explicitly accepted D's
6m59.375s live comparison duration in place of the planned 8–10 minutes for
this run. Host validation, native build and Standards/Spec reviews passed
before the hardware test. All 2381 D diagnostic records persisted without
loss; input polling averaged 4ms with a 7.031ms maximum gap and no polling or
send failure. Full comparisons are in the [accepted result](LATENCY_COMBINED_RESULT.md).

Numerical physical input-to-screen latency remains unmeasured. Reception
conditions varied between tests. Audio queue residence still rebuilt after a
late arrival gap, and native displayed-picture age remains unknown. Acceptance
establishes the tested playable baseline, not resolution of those uncertainties
or behavior beyond the recorded sessions. Video/audio/quality and control
policies remain those accepted in the test.

## Build profile

With the pinned dependencies and VitaSDK toolchain available, the accepted
Release configuration used these non-secret options:

```sh
cmake -S vita-client -B build/vita \
  -DCMAKE_BUILD_TYPE=Release \
  -DPLUS_VITA_DIAGNOSTIC_APP=ON \
  -DPLUS_VITA_PUBLIC_APOLLO_TEST=ON \
  -DPLUS_VITA_INPUT_WORKER=ON \
  -DPLUS_VITA_DEFERRED_LOG=ON
cmake --build build/vita --parallel
```

The build output is `build/vita/plus_vita_smoke.vpk`; the accepted artifact was
retained under its distinct filename above. Source defaults for the input
worker and deferred logging remain OFF, so an unspecified default build does
not select this accepted profile. A future rebuild must be identified and
checked separately; configuration alone does not promise an identical hash.

The diagnostic entry flag selects the historical text interface after the
graphical startup work. Normal new builds otherwise enter the graphical app.
It does not change the bytes or acceptance of the retained package above.

## Phase closure

GitHub [spec #1](https://github.com/Calligula87/plus-vita/issues/1) and all its
execution tasks #2–#12 are complete, including the
[combined hardware gate #12](https://github.com/Calligula87/plus-vita/issues/12).
The [specification](LATENCY_SPEC.md), [task index](LATENCY_TASKS.md),
[test plan](LATENCY_TEST_PLAN.md) and
[ADR 0001](adr/0001-preserve-stream-baseline-during-input-latency-work.md)
record the agreed scope and decisions. No further hardware test is required
to close this phase. Further improvements and release preparation can start
from this identified, accepted package.
