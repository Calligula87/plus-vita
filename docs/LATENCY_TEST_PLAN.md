# Latency investigation test plan

Status: updated on 2026-10-14; diagnostic software checks passed and a clean
5m52.645s hardware run was inspected. The full 8–10 minute gate and subjective
comparison remain unconfirmed; see [first result](LATENCY_BASELINE_RESULT.md).
The [specification](LATENCY_SPEC.md) defines scope and user
stories; [ADR 0001](adr/0001-preserve-stream-baseline-during-input-latency-work.md)
records the decision. The [baseline procedure](LATENCY_BASELINE_TEST.md) is the
step-by-step installation/log collection guide and metric dictionary.

## Builds and gates

| Build | Role | Current evidence |
| --- | --- | --- |
| S: `live-720p-refs5-video-reorder-1` | Accepted stream build and rollback | Accepted hardware run; verified VPK SHA256 `6F4FEB9A3FAC9F90BC201AF0110DAA28979DC130E321F7CBE5F8BFA0D313E56B`. |
| B: `live-720p-refs5-latency-baseline-1` | Diagnostic stream build, current test package | Release VPK built; host checks passed. SHA256 `B48C54FDF8616512CCB755E4502819BB10EEF8E1EEC7DEEA6FC8B92BEAF1EC23`. Correct installed build ran cleanly for 5m52.645s; full acceptance pending. |
| C: independent-input candidate | Measure the effect of input isolation | Not implemented; build tag/hash must be recorded when packaged. |
| L: deferred-logging candidate | Measure moving diagnostic persistence off hot paths | Not implemented; separate from C, with its own build tag/hash. |

1. **Diagnostic gate:** B preserves S's experience and produces interpretable
   measurements. Revert to S and investigate instrumentation if B regresses.
2. **Analysis gate:** identify measured local delays and uncertainty before
   building C. Poll cadence and achievable improvement remain hypotheses.
3. **Candidate software gate:** input-to-feedback behavior and lifecycle checks,
   sanitizers and VitaSDK packaging pass before C's hardware comparison.
4. **Adoption gate:** repeatable response improvement plus an 8–10 minute stable
   candidate run. Ambiguous or regressed results retain S; C is not accepted by
   default. B remains the measurement comparator, not an automatically promoted
   replacement for S.

Triage clarification: the clean first run provides sufficient evidence for
analysis and software preparation, at the maintainer's request. Unfinished
baseline acceptance blocks hardware comparison/adoption directly, not software
implementation. Thus #5 and #8 can be prepared while #3 remains open. Compare
C and L separately against B; no combined first candidate.

## Test boundaries

The highest boundary is the real Vita session: physical control through the
stream to the visible game response. Use the same Vita, game, scene, Wi-Fi
location, stream settings and response action. Prefer an immediate, repeatable
menu selection response over an action whose game animation duration varies.

The software boundary is sampled control through the actual feedback sender to
captured outgoing state/history. Retain real mapping, serialization and worker
lifecycle wherever available. A fake video stall is useful only if it exercises
the future production input owner's actual loop. Histogram/logger tests support
the validity of the measurements; they cannot establish the user's latency.

## Automated and packaging cases

| ID | Observable case | Expected result | Evidence/status |
| --- | --- | --- | --- |
| A01 | Known duration distribution, a 90ms poll gap and bucket boundaries | Correct counts/totals, visible outlier and conservative percentile upper bounds | Passed in the existing latency harness. |
| A02 | Million-sample totals, backward span and empty metric | Accurate 64-bit total; invalid span counted without wrap; empty count explicit | Passed. |
| A03 | Concurrent diagnostic writers, stop and next collection | Complete serialized writes; shutdown summaries excluded; counters reset | Passed. |
| A04 | Sensitive synthetic diagnostic value | Existing redaction retained; value absent from output | Passed. |
| A05 | Actual generated sender versus upstream: sticks, buttons, triggers, touch slot replacement and releases | Matching captured feedback sequence; existing release/down semantics retained | Passed for the exercised sequence. |
| A06 | Duplicate updates, injected local send failure, rapid updates and next sender session | Dedup preserved; failure observed; publications reconcile with consumption/coalescing; reset after join | Passed. |
| A07 | Current instrumentation under memory/undefined-behavior and thread sanitizers | No reported failure in the exercised host paths | Passed on 2026-10-14. |
| A08 | Diagnostic Release VPK and rollback package | Package builds; manifest identifies build/hash; S hash retained | Passed. The retrieved hardware log now identifies installed B. |
| A09 | Production input owner while video work is blocked | New samples reach the existing sender independently; no busy-spin or dependence on video progress | Passed host: actual decoder stalled at native API while feedback and terminal stop continue. |
| A10 | Input owner: held controls/touches, stop chord, init/send failure, disconnect and repeated session | Existing mappings/releases preserved; error reaches session owner; worker joins before feedback/transport destruction; no use after shutdown | Host mappings/lifecycle/failure/restart passed; native preview/disconnect/touch restoration remain hardware checks. |
| A11 | Candidate sanitizer and packaging run | A05/A06 equivalence and new lifecycle checks pass; new build/hash recorded | C memory/thread checks and Release VPK passed; FTP staging pending. |
| A12 | Actual diagnostic producers against a controlled slow/failing writer | No producer waits on disk or disk-held locks; bounded retention, ordering/overflow and failure outcomes explicit | Passed L: blocked real sink, bounded loss/error accounting and persistence separation. |
| A13 | Deferred diagnostics under concurrent stop/restart, redaction and capacity pressure | No unbounded memory/wait, deadlock, sensitive retention or silent record loss; terminal diagnostics accounted for | L memory/thread checks passed, including stuck-writer shutdown and partial init. Hardware still pending. |
| A14 | Audio arrival-to-output reproduction with controlled pacing/sequence/logging | Competing backlog hypotheses tested without changing accepted playback policy | Passed agent reproduction/investigation; hardware cause remains unresolved, no playback fix claimed. |

Existing commands, after configuring/building the Vita target:

```sh
sh tools/test-latency.sh
PLUS_VITA_TEST_SANITIZERS=thread sh tools/test-latency.sh
sh tools/test-input-worker.sh
sh tools/test-deferred-log.sh
sh tools/test-audio-queue.sh
# Repeat these three with PLUS_VITA_TEST_SANITIZERS=thread for concurrency checks.
```

The script compiles the generated sender actually used by the VPK. Receiver and
FEC host checks already exist; rerun them when relevant code changes, rather
than treating an unchanged receiver test as proof of input responsiveness.

## Hardware procedure

| ID | Procedure | Expected evidence |
| --- | --- | --- |
| H01 | Install B through VitaShell, not upload alone; verify its build tag in the result log | Correct diagnostic executable identified; package hash recorded. |
| H02 | In the chosen scene, make ten separated D-pad/button presses and observe the first visible response | Same response definition for later comparisons; subjective notes and optional recording. |
| H03 | Play normally for 8–10 minutes, exercising both sticks, ordinary buttons, rear L2/R2/L3/R3 and front touch | No new picture/audio/control regression, stuck control, decoder failure, queue-overflow stop or unexpected disconnect. |
| H04 | Release held controls and touches; stop with START + SELECT for two seconds | Clean shutdown; no persistent held controls or unwanted exit-chord behavior. |
| H05 | Retrieve the allocation log before another launch, then start and stop a subsequent session | Complete identified timing summary preserved; restart works with reset counters. |
| H06 | After baseline analysis and software checks, repeat H01–H05 with C in the same scene/setup and duration | Comparable results with a unique candidate tag/hash and identical metric semantics. |
| H07 | Compare B and C, repeating a short response comparison in reversed order if the result is ambiguous | Repeatable perceived or measured response improvement; uncertainty and network variation recorded. |
| H08 | If B or C regresses, install preserved S and verify normal operation | Working rollback retained; experiment is not promoted. |
| H09 | After #3 and #8, compare L alone with B using identified builds, response actions, 8–10 minutes and stop/restart | Producer/persistence costs, record losses and stream/control/audio behavior compared; verdict with uncertainty, #10. |

Do not force a network failure on the Vita solely to exercise teardown. Host
fault injection covers controllable error paths; record naturally occurring
disconnect/recovery behavior when it happens. If B differs noticeably from S,
repeat S/B under comparable conditions to distinguish overhead from changing
network conditions before proceeding to C.

## Optional response measurement

At 120 or 240 recording fps, film the physical button and screen together for
the same ten presses. Keep definitions of press onset and first visible
response consistent. Compute each estimate as frame difference times
1000/recording fps, then compare median and range. A recording frame represents
about 8.3ms or 4.2ms respectively; press-contact ambiguity and scanout add
uncertainty. A difference below that uncertainty is inconclusive.

If recording is unavailable, record whether each repeated comparison feels
better, unchanged or worse, with scene and control action. This can support a
user acceptance decision, but report that no numerical input response latency
has been measured. A decrease in local poll gaps alone is not an end-to-end
measurement.

## Results and acceptance

Save raw logs and optional recordings only in ignored private local artifacts.
Publish sanitized aggregate results in the task, without credentials, addresses,
account/game identifiers, session material, payloads or raw control histories.

| Result field | What to record |
| --- | --- |
| Identity | S/B/C role, build tag, VPK hash, actual installation confirmed. |
| Test conditions | Same or changed scene/setup, approximate duration, response action and number of samples; keep identifying details private. |
| Response | Median/range with recording fps and uncertainty, or clearly labeled subjective repeated comparison. |
| Input path | Poll-gap and sample/publication/wake/send counts, mean, percentile upper bounds, max; failed sends, coalesced and invalid samples. |
| Video/logging | Queue age, callback/main waits, assembly, decode, display and write/flush costs; label submission intervals accurately. |
| Stability | Stops/errors, queue peak/overflows, recovery pauses and incomplete/FEC events per minute, audio failures, control correctness and restart. |
| Verdict | Diagnostic gate pass/fail/inconclusive, then candidate adoption accept/reject/inconclusive with reasons and next task. |

Recovery and loss counts need context: a quieter network run does not establish
a causal improvement. Native output has no per-AU timestamp in this baseline;
the audited decoder retained pictures, so submission intervals are not the age
of the picture being shown. Empty metrics and unavailable hardware evidence
must remain explicitly unknown.

No absolute millisecond target has been agreed. Accept C only when the
responsiveness comparison is repeatably better and there is no new quality,
audio, control or stability regression. If only a local timing improves, record
that limited finding and keep the overall input-response claim unverified.

## User-story coverage

| Stories in the specification | Primary cases |
| --- | --- |
| 1, 10–14 | H02, H06–H07 and optional response measurement. |
| 2–4 | H03, H06–H08 and normalized stability counters. |
| 5–8 | A05, A09–A10, H03–H05. |
| 9, 15, 26 | A08, A11, H01, H05, H08. |
| 16–19, 22–24 | A01–A03, A05–A07, A09–A11 and baseline analysis. |
| 20–21 | Results interpretation and explicit native-output/measurement limits. |
| 25 | A04 and sanitized result publication. |
| 27–28 | All four gates, dependency tracking, ADR and evidence-based adoption. |
