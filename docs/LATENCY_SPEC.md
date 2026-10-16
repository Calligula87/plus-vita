# Improve Vita input response while preserving the accepted stream

Canonical spec: [GitHub issue #1](https://github.com/Calligula87/plus-vita/issues/1).

## Problem Statement

The Vita stream is stable with relatively few lost frames, but the delay
between a physical control input and the visible streamed game response is
noticeable. The player cannot yet quantify that delay, and removing buffering
without evidence risks losing the picture quality, audio and stability already
achieved.

At the start of this work, the implementation sampled input in the same loop
that decoded video and waited for display. That was a plausible contributor,
not an established
explanation for the entire input response latency.

## Solution

Establish a diagnostic stream build that preserves the accepted experience,
measure it on the Vita in a repeatable scene, and then evaluate independent
input sampling with identical diagnostics. Retain the change only after a
repeatable response improvement and an 8–10 minute stability test, with the
accepted stream build preserved for rollback.

The user confirmed quality/stability as the priority, availability for Vita
comparison tests, optional phone slow-motion measurement, and this staged scope.

## User Stories

1. As a Vita player, I want my control input to produce a visible game response sooner, so that streamed games feel more responsive.
2. As a Vita player, I want latency work to preserve current picture quality, so that responsiveness does not come at an unwanted visual cost.
3. As a Vita player, I want the stream to retain its current stability, so that a response improvement does not introduce freezes or disconnects.
4. As a Vita player, I want audio quality and continuity preserved, so that control improvements do not damage sound.
5. As a Vita player, I want existing buttons and stick mappings preserved, so that I can play without relearning controls.
6. As a Vita player, I want rear L2/R2/L3/R3 and front touch behavior preserved, so that games remain fully controllable.
7. As a Vita player, I want button and touch releases handled reliably, so that a stop or reconnect does not leave a control held.
8. As a Vita player, I want START + SELECT to keep stopping the session safely, so that I retain a dependable exit action.
9. As a Vita tester, I want every result tied to an installed build and package hash, so that comparisons use the intended executable.
10. As a Vita tester, I want a short, repeatable baseline procedure, so that I can collect useful evidence without specialist equipment.
11. As a Vita tester, I want to compare the same game scene and Wi-Fi setup, so that unrelated conditions are less likely to explain the difference.
12. As a Vita tester, I want an optional phone measurement method, so that I can estimate input response latency without a laboratory rig.
13. As a Vita tester, I want the recording uncertainty stated, so that apparent changes smaller than a recording frame are not overstated.
14. As a Vita tester, I want a comparison method when video recording is unavailable, so that testing can still proceed with clearly limited evidence.
15. As a Vita tester, I want the allocation log saved before another launch overwrites it, so that a completed test is not lost.
16. As a maintainer, I want input polling gaps measured, so that I can see when the next sample is delayed by video work or scheduling.
17. As a maintainer, I want sample publication and sender wake/send intervals distinguished, so that I can locate local input delays accurately.
18. As a maintainer, I want coalesced updates and failed sends counted, so that successful timing samples do not conceal those outcomes.
19. As a maintainer, I want queue, decode, display and logging costs measured separately, so that the next intervention follows evidence.
20. As a maintainer, I want local submission intervals distinguished from input response latency, so that reports do not claim an unmeasured end-to-end gain.
21. As a maintainer, I want native decoder buffering uncertainty recorded, so that a submission timestamp is not mistaken for the displayed picture's timestamp.
22. As a maintainer, I want bounded telemetry and summaries outside active gameplay, so that diagnostics have a limited effect on the stream being measured.
23. As a maintainer, I want controller feedback compared at the transport boundary, so that timing instrumentation preserves the existing protocol behavior.
24. As a maintainer, I want worker startup, failure, stop and restart tested, so that input isolation does not add races or resource-lifetime failures.
25. As a maintainer, I want measurements to avoid new input values, payloads or credentials in logs, so that diagnostic sharing stays within the existing project boundaries.
26. As a Vita player, I want the verified accepted package available for rollback, so that an unsuccessful experiment cannot strand me on a worse build.
27. As a maintainer, I want dependencies and acceptance gates tracked explicitly, so that an untested candidate is not promoted as an improvement.
28. As a maintainer, I want useful reference-project techniques evaluated against this client, so that another project's success is not mistaken for local validation.

## Implementation Decisions

- Preserve the accepted video profile/allocation, decoder guards, reference
  continuity, FEC/reorder, synchronous presentation/catch-up, audio settings,
  mappings, stop chord and recording-off behavior during this experiment.
- Add fixed-size duration histograms to the existing input, feedback, receiver,
  decode/presentation and diagnostic-logging boundaries. Store aggregate
  durations and outcomes; add no packet payloads or control values.
- Keep existing diagnostic write/flush behavior for the first baseline and
  measure its cost. Emit new summaries after receive/feedback workers stop.
- Use monotonic local time. Report counts, means, maxima and explicitly labeled
  percentile bucket upper bounds. Empty or invalid samples are not zero latency.
- Retain event-driven feedback and its existing wire behavior. The 200ms idle
  timeout is a keepalive, not an active-input scheduling delay to remove.
- Attribute input timings to the latest consumed sample and expose coalescing
  and failed sends. Local send success does not establish server receipt.
- Treat submission-to-presentation spans as local submission intervals. Native
  output can belong to an earlier access unit; no physical scanout or exact
  displayed-picture age is established by these spans.
- Evaluate an independent, bounded-cadence input owner only after the diagnostic
  baseline is collected and analyzed. Preserve mappings and the existing
  feedback sender; avoid busy spinning and new coupling to video waits.
- Establish explicit startup/terminal-state ownership, neutral-state handoff,
  worker error propagation and join ordering before feedback/transport teardown
  or touch-state restoration. Do not claim a neutral update was received remotely
  without an acknowledgement.
- Use the same diagnostics in the input candidate. Select the polling cadence
  from the baseline and hardware scheduling evidence; no exact period or
  numerical end-to-end gain has been agreed.
- Keep the verified accepted package available. Adoption is a separate decision
  after the comparison gate, not a consequence of compiling successfully.
- Following the maintainer's triage request, use the interpretable first hardware
  run to complete analysis and unblock software implementation. Keep unfinished
  8–10 minute/subjective/restart baseline checks as direct prerequisites of
  hardware comparison and adoption, rather than blocking software preparation.
- Evaluate deferred diagnostic persistence as a separate logging-only candidate;
  retain bounded storage, redaction, explicit overflow/failure accounting and
  comparable timing semantics. Do not combine it with the first input candidate.
- Investigate the already-existing audio backlog separately, without changing
  accepted playback settings or implying that audio residence is visual latency.

## Testing Decisions

- Prefer tests at externally observable boundaries: the real Vita session for
  input response latency, and actual controller feedback at the transport
  boundary for mapping/protocol behavior. These seams were confirmed in Q4.
- Reuse the existing actual generated-sender versus upstream-sender harness,
  changing the transport to a capture boundary while retaining serialization,
  state diffing and thread lifecycle. Compare button, stick, trigger, touch,
  release, duplicate and failure behavior; test coalescing and session restart.
- Retain focused duration-aggregation and concurrent-logger tests because bad
  counters would invalidate hardware conclusions. Cover known distributions,
  stalls, boundary values, large totals, backward spans, exclusion/reset and
  redaction. These tests validate measurement behavior, not user-visible speed.
- For the future input owner, extend the highest available input-to-feedback
  seam with controllable sampling/time and video stalls. Prove sampled updates
  can reach the existing sender while video is blocked, plus clean lifecycle
  and mapping equivalence. Do not test private helper arrangements as the goal.
- Use AddressSanitizer/UndefinedBehaviorSanitizer and ThreadSanitizer for the
  exercised host behavior, plus VitaSDK compilation/VPK packaging. Existing
  receiver/FEC tests are prior art and are required again if their behavior changes.
- Run the diagnostic baseline and candidate on the same Vita, game/scene and
  Wi-Fi setup, including ten separated visible-response presses and an 8–10
  minute normal-play run. Test short presses, held controls, touch releases,
  stop, and a subsequent session. Do not manufacture transport faults on hardware
  merely to exercise error handling already covered by host tests.
- Optional 120/240fps filming estimates input response latency with frame and
  physical-press uncertainty. Otherwise record consistent subjective comparisons
  and state that no numerical end-to-end measurement is available.
- Compare distributions and normalized recovery/queue/audio counters. Repeat
  inconclusive or visibly confounded comparisons; network loss can differ
  between runs. No numerical latency budget is invented before measurement.
- Pass the adoption gate only with repeatable response improvement and no new
  queue-overflow stop, decoder/audio failure, stuck/missing control, picture
  degradation or unclean shutdown. Local timing gains alone do not prove a
  game-response gain. Ambiguous results retain the accepted stream build.

## Out of Scope

- Restarting the failed display-worker experiment or changing presentation
  ownership, catch-up policy, FEC/reorder grace or compressed-frame drop policy.
- Reducing resolution, bitrate or picture quality; changing accepted audio.
- Replacing the cloud protocol, changing authentication or claiming changes to
  Sony's game processing/encoder/network behavior.
- Reinterpreting decoder reference slots as output latency, or claiming exact
  displayed-frame age without a correlated native output timestamp.
- Mandatory phone recordings, a new in-game UI/overlay, automatic audio/video
  captures, or additional control-value/credential logging.
- Publishing private logs or captures, deploying another candidate before the
  baseline gate, or treating successful host tests as hardware acceptance.

## Further Notes

On 2026-10-14 the diagnostic package was implemented locally and built; its
AddressSanitizer/UndefinedBehaviorSanitizer and ThreadSanitizer checks passed,
and exercised controller feedback matched upstream. It was uploaded with verified
readback and the installed build completed a clean 352645ms Vita run. All 21
metrics were present: polling gap mean/max 15.812/57.513ms, sampled-input send
means 0.357–0.427ms and diagnostic write/flush mean/max 10.565/35.083ms across
2461 writes (approximately 26s aggregate). No native/input/output failure was
found. Existing audio residence averaged 230.748ms, versus 250.148ms in the
previous accepted run; it is not a newly established regression.

The scoped phase is complete. Independent input sampling and deferred logging
are implemented, tested, reviewed and committed on `main`.
Baseline B was accepted, C improved perceived response,
L validated bounded diagnostics, and the separately identified combined D
retained C's response/quality/control/restart experience. All 2381 D diagnostic
records persisted without loss. D is the accepted working build; C is the
immediate fallback and original B/L/S packages remain preserved. See
[combined result](LATENCY_COMBINED_RESULT.md) for exact identities and evidence.

The maintainer explicitly accepted each recorded comparison duration: B
7m46.418s, C 7m18.286s, L 5m02.205s and D 6m59.375s. These are scoped revisions
of the original 8–10 minute gate, not claims of longer runs. All requested
picture/sound/control/release/stop/restart checks are complete. Numerical
physical input response remains unmeasured; native picture age and the cause
of variable audio backlog remain uncertain. Quieter reception in D confounds
presentation/audio comparisons. No video/audio/quality policy was changed,
and the rejected display-worker experiment was not reopened.

Canonical specification and execution tasks are published in GitHub Issues.
This document is a local snapshot. Supporting repository documents are
`docs/LATENCY_TEST_PLAN.md`, `docs/LATENCY_BASELINE_TEST.md`, and ADR 0001;
their evidence and decisions are committed on `main`. The maintainer subsequently
accepted this package as the [playable milestone](PLAYABLE_MILESTONE.md) for
release preparation and authorized publishing the code and documentation.

Primary-source references inspected during planning:

- [Vitaki input thread and pacing](https://github.com/mauricio-gg/vitaki-vitarps5/blob/468638e70a8899b000196d5d73b1d35e221ba968/vita/src/host_input.c#L651-L668).
- [GreenVita input sampling during display pacing](https://github.com/Day-OS/green-vita/blob/ae2625d295b4fba005a769b1309fd70dcd6cb63f/src/shell/mod.rs#L242-L266).
- [Pylux event-driven feedback sender](https://github.com/ForWard-Technologies-LLC/Pylux/blob/ca87a49a12bab071948f565ca53ffdad790775d3/lib/src/feedbacksender.c#L348-L398).
- [OpenNOW timing counters](https://github.com/OpenCloudGaming/OpenNOW-vita/blob/654a9356a9f01df4a67ea802b8f8c96af7ea2ca3/src/gfn/peer.rs#L912-L986).

These references inform experiments; their performance is not a verified result
for this client.

### Execution tasks

- [x] [#2 Deliver diagnostic baseline and software validation](https://github.com/Calligula87/plus-vita/issues/2) — implemented, tested, committed locally and packaged.
- [x] [#3 Complete remaining diagnostic baseline checks](https://github.com/Calligula87/plus-vita/issues/3) — baseline accepted, including explicit duration revision and remaining checks.
- [x] [#4 Analyze baseline delays and select the input experiment](https://github.com/Calligula87/plus-vita/issues/4) — available evidence analyzed; full hardware acceptance excluded.
- [x] [#5 Isolate input sampling and validate its lifecycle](https://github.com/Calligula87/plus-vita/issues/5) — implemented, tested, reviewed and packaged.
- [x] [#6 Compare independent input sampling on Vita](https://github.com/Calligula87/plus-vita/issues/6) — better perceived response, preserved quality/controls/restart; duration accepted.
- [x] [#7 Record candidate acceptance or rollback](https://github.com/Calligula87/plus-vita/issues/7) — C accepted; fallback packages preserved.
- [x] [#8 Defer diagnostic file I/O outside streaming hot paths](https://github.com/Calligula87/plus-vita/issues/8) — implemented, tested, reviewed and packaged separately.
- [x] [#9 Investigate existing audio queue backlog](https://github.com/Calligula87/plus-vita/issues/9) — scoped investigation complete; hardware cause unresolved and playback policy unchanged.
- [x] [#10 Validate deferred logging on Vita](https://github.com/Calligula87/plus-vita/issues/10) — L validated without diagnostic loss or reported regression; duration accepted.
- [x] [#11 Prepare combined input and logging candidate](https://github.com/Calligula87/plus-vita/issues/11) — joint/full checks, two-axis review, native Release and verified FTP staging complete.
- [x] [#12 Validate combined build on Vita](https://github.com/Calligula87/plus-vita/issues/12) — D accepted: response at least as good as C, everything worked, zero diagnostic loss; duration explicitly accepted.

Native GitHub sub-issue/dependency relationships are canonical. Rejecting a
candidate completes an experiment, not the parent's response-improvement goal;
keep the parent open and record the next evidence/work unless the user changes
that goal.

Here the input improvement was accepted and retained by the combined build,
so parent #1 completes this scoped phase. No additional mandatory test remains
for that acceptance; further tuning would require a separate experiment.
