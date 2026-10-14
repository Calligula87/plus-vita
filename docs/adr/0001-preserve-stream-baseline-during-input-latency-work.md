---
status: accepted
date: 2026-10-14
---

# Preserve the accepted stream while measuring and isolating input

The Vita stream is now acceptable for stability, but input response latency is
noticeable and its dominant cause is unmeasured; a previous display-worker
experiment passed host checks but failed on hardware with queue saturation.
We will measure a diagnostic stream build first, then evaluate independent
input sampling against the same measurements while retaining the accepted
video, FEC/reorder, audio and control policies.
The user confirmed this order and prioritized quality and stability over
aggressive buffering or quality changes.

## Considered options

| Option | Decision and reason |
| --- | --- |
| Measure the accepted path, then isolate input sampling | Chosen: separates the effect of instrumentation from the input change and retains a tested rollback. |
| Restart display-worker or buffer-pool work immediately | Deferred: the failed hardware experiment has an unresolved cause; success in another client does not validate this implementation. |
| Shorten reorder grace, drop compressed reference frames, or reduce quality | Excluded from the first experiment: these can undo the recovered continuity and stability, and the baseline does not show a continuous reorder delay. |

## Consequences

- The diagnostic stream build must pass its hardware gate before the input
  candidate is evaluated. Host checks and packaging cannot satisfy that gate.
- The input candidate keeps one owner of controller/touch sampling, the
  existing mappings and feedback protocol, and explicit neutral-state,
  stop and join ordering while the transport is still usable.
- The accepted stream build and its verified package remain available for
  rollback. A candidate becomes accepted only after repeatable response
  improvement and an 8–10 minute run without new quality or control regressions.
- Counters are bounded and new summaries are written after streaming stops.
  Input response latency requires a repeatable visible response; local
  submission intervals, ping and dropped-frame counts cannot substitute for it.
- Native decoder buffering remains an uncertainty. The audited replay retained
  three pictures for draining; a submission producing output does not prove
  that the output belongs to that submission.

This decision records the current scope; it does not establish the optimal
polling period or guarantee an improvement from a separate input worker.

## Triage clarification, 2026-10-14

After a clean 352645ms diagnostic run, the maintainer requested that measured
findings become active improvement tasks. Analysis and software preparation
may proceed from this interpretable evidence while remaining baseline checks
stay open; the full baseline gate still precedes candidate hardware comparison
and adoption. This does not mark the shorter run as full acceptance.

Independent input sampling remains the first software experiment. Synchronous
diagnostic I/O, now measured at about 26s aggregate, is a separate logging-only
candidate with its own hardware comparison. Investigate the pre-existing audio
backlog without changing accepted playback policy. Do not combine the first
input and logging comparisons, or reopen display-worker work under this decision.

Related evidence: [display experiment and rollback](../DISPLAY_PACING_TEST.md),
[accepted reorder result](../VIDEO_FEC_TEST.md),
[decoder evidence](../720P_DECODER_PLAN.md),
[baseline procedure](../LATENCY_BASELINE_TEST.md), and
[test plan](../LATENCY_TEST_PLAN.md).
