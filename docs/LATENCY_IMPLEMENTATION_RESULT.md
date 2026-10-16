# Latency software delivery

2026-10-15. Software preparation for GitHub #5, #8 and #9, followed by completed
physical gates #3/#6/#7/#10/#12. [Combined D is now accepted](LATENCY_COMBINED_RESULT.md).
The sections below retain the delivery and review evidence leading to acceptance.

Changes are committed on the existing `main` branch: `82df29a` records the
diagnostic foundation, `fda4046` implements the isolated candidates and audio
investigation, and `e299abb` resolves the input review findings. Local commits
are not automatically pushed or published. User-owned agent configuration was
left separate.

| Delivery | Result / evidence |
| --- | --- |
| C: independent input | Requested 4ms sampler, unchanged mappings/touch identity, deduplication, native timestamp repeat/change counters, explicit fatal/send error and neutral/join ownership. [Procedure and limits](LATENCY_CANDIDATE_TEST.md). |
| L: deferred logging | Separate Release configuration, bounded normal/critical storage, redaction before retention, sequence ordering, explicit overflow/truncation/failure counters and two-second shutdown ownership. Baseline input policy retained. |
| Audio investigation | Real unchanged arrival/queue/output code exercised with controlled external boundaries; [observations, competing explanations and next experiment](AUDIO_QUEUE_INVESTIGATION.md). No playback setting changed. |
| Rollback / diagnostic baseline | Original packages retained. Successful compilation is not hardware acceptance. |

The final Release configurations are C (`INPUT_WORKER=ON`, `DEFERRED_LOG=OFF`),
L (`OFF`, `ON`) and default (`OFF`, `OFF`). At isolated delivery CMake rejected
`ON`, `ON`; after #10 validated L, #11 introduces the uniquely identified
experimental combined profile. See [joint test](LATENCY_COMBINED_TEST.md). The local
package manifest records final hashes, byte sizes, source commit and FTP status.
Never infer installation from upload or from a build tag in an untested package.

Staging completed after the server became available: C/L and the original B/S
packages are in `ux0:data/`, all four verified by SHA256 readback. The existing
primary diagnostic VPK was backed up and its B hash verified; it and the prior
log were preserved. No package was installed automatically. Software tasks
#5/#8/#9 are closed. Subsequently #3 was completed using the accepted baseline
duration and the maintainer's remaining-test confirmation, supported by a
distinct clean subsequent baseline session. C subsequently passed its
comparison: the maintainer reported better response, equivalent picture/sound/
controls/releases/restart, and explicitly accepted its 7m18.286s duration.
#6/#7 are complete; identified C is accepted with B/S retained. #10 logging-only
L versus original B subsequently passed, then combined D passed #12, completing
the scoped parent task. See the
[C hardware result](LATENCY_INPUT_WORKER_RESULT.md) and ADR acceptance update.

## Automated validation

The available host suite covers histogram bounds/backwards clocks, concurrent
synchronous diagnostics, generated-sender wire equivalence with upstream,
input mappings/worker lifecycle, deferred sink boundaries, controlled audio
arrival/output models, H.264 configuration, 200000-frame reorder sequences,
64 FEC reference vectors and 15 independent controlled-loss parity cases.
Address/undefined sanitizers cover these paths; thread sanitizers cover timing,
diagnostics, actual input/feedback and audio reproduction concurrency.

The input harness now blocks the actual production decoder at its native
`sceAvcdecDecode` boundary and verifies real feedback plus terminal stop and
neutral/feedback join while decode remains blocked. Another test blocks the
actual synchronous file sink, injects a send error that emits nested transport
diagnostics, and requires input/feedback shutdown before releasing disk.
Logger tests include bounded shutdown with a stuck writer, critical/ordinary
overflow, truncation, redaction, write failure, thread-create failure and restart.

Host logs and build logs are private ignored artifacts under `build/implement/`.
Known upstream warnings remain; these checks do not emulate Vita scheduling,
native network behavior, touch restoration or physical button-to-screen delay.

## Standards

Review fixed point: `82df29a7557c8f7f45695c582d61f432dae23838`.
Standards review of the implementation and follow-up: **0 findings**. No
documented-standard violation, actionable smell or ADR conflict was found.

## Spec

Initial review: **2 findings**, both resolved in `e299abb`:

- Sender failure diagnostics could hold its state mutex across synchronous
  disk I/O. C now suppresses/counts nested sender-thread messages before the
  sink; registration is retired on every current thread exit. B/L keep their
  diagnostic policy. A blocked real sink regression test verifies this path.
- The first blocked-video fixture used an unrelated test mutex. The replacement
  invokes production decode and stalls its actual SDK boundary while exercising
  input and terminal shutdown.

Follow-up Spec review: **0 remaining findings**. Full Vita preview, native
touch restoration, repeated sessions and physical response/stability required
the later hardware gates, which subsequently passed. Standards: 0; Spec: 2
fixed, 0 remaining.

## Final combined acceptance

Combined implementation `5ff453b`, reviewed from `06a74ea`, passed the full
available host suite, relevant isolated/joint thread checks, native Release and
four-profile identity checks. Standards and Spec reviews each had zero findings.
The identified D VPK was uploaded and read back with matching SHA256, then
tested on Vita for 6m59.375s. The maintainer explicitly accepted that duration
and confirmed response at least as good as C and everything worked, including
picture/sound/controls/releases/stop/restart. All 2381 records persisted, with
zero loss; no input/send failure occurred and shutdown completed cleanly.

D is the accepted working package, C the immediate fallback. The
[final result](LATENCY_COMBINED_RESULT.md) records hashes, local timings,
reception confounds and the remaining audio buildup. The accepted phase does
not claim numerical physical input response or a fixed audio backlog. Source
defaults remain unchanged; acceptance requires no package change or new upload.
