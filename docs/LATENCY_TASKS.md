# Latency work: task index

Canonical specification: [#1](https://github.com/Calligula87/plus-vita/issues/1).
Tasks are GitHub sub-issues with native dependencies; this is a local status
snapshot dated 2026-10-14, not a second issue tracker.

Delivery update, 2026-10-15: **#5 input isolation**, **#8 separate deferred
logging** and **#9 audio investigation** are implemented/reviewed locally.
Full host memory/undefined and relevant thread-sanitizer suites passed; input,
logging and default Release builds passed and a combined candidate is rejected.
Source commits: `fda4046`, `e299abb`; foundation: `82df29a`. They have not been
pushed automatically. The historical triage table below is superseded by this
delivery update for #2/#5/#8/#9.

FTP staging/readback is complete for C, L, original B and rollback S; all four
SHA256 hashes were verified. No package was installed or accepted. The user
confirmed equivalent diagnostic quality/stability with noticeable input delay;
#3 still needs its 8–10 minute run and restart. #6/#7/#10 remain open hardware
comparison/acceptance gates. See [delivery/review](LATENCY_IMPLEMENTATION_RESULT.md),
[candidate procedure](LATENCY_CANDIDATE_TEST.md), and
[audio investigation](AUDIO_QUEUE_INVESTIGATION.md).

| Task | State after results triage | Dependency / owner |
| --- | --- | --- |
| [#2 Deliver diagnostic baseline and software validation](https://github.com/Calligula87/plus-vita/issues/2) | Completed locally; issue closed | Foundation committed in `82df29a`; no hardware acceptance. |
| [#3 Complete remaining diagnostic baseline checks](https://github.com/Calligula87/plus-vita/issues/3) | Open; follow-up clean 7m46.418s, duration explicitly accepted | Front touch/rear controls, releases, response actions and restart still unconfirmed; no repeat long run required. |
| [#4 Analyze baseline delays and select the input experiment](https://github.com/Calligula87/plus-vita/issues/4) | Analysis completed; issue closed | Available evidence analyzed, not full acceptance; `ready-for-agent`. |
| [#5 Isolate input sampling and validate its lifecycle](https://github.com/Calligula87/plus-vita/issues/5) | Software delivered; issue closed | C built/reviewed/upload verified; hardware acceptance is #6/#7. |
| [#6 Compare independent input sampling on Vita](https://github.com/Calligula87/plus-vita/issues/6) | Open | Blocked by #3 and #5; `ready-for-human`. |
| [#7 Record candidate acceptance or rollback](https://github.com/Calligula87/plus-vita/issues/7) | Open | Blocked by #6; `ready-for-agent`. |
| [#8 Defer diagnostic file I/O outside streaming hot paths](https://github.com/Calligula87/plus-vita/issues/8) | Software delivered; issue closed | L built/reviewed/upload verified; hardware acceptance is #10. |
| [#9 Investigate existing audio queue backlog](https://github.com/Calligula87/plus-vita/issues/9) | Scoped investigation complete; issue closed | Hardware cause uncertain; playback policy unchanged. |
| [#10 Validate deferred logging on Vita](https://github.com/Calligula87/plus-vita/issues/10) | Open | Blocked by #3 and #8; `ready-for-human`. |

Readiness labels describe who can execute the defined task, not whether a
blocked task can be started immediately. No assignee was invented.
Every task has category `enhancement` and exactly one triage state. The input
and logging candidates are separate; do not combine their first comparisons.

Before the next test, use the [test plan](LATENCY_TEST_PLAN.md) and
[baseline procedure](LATENCY_BASELINE_TEST.md). The
[local spec snapshot](LATENCY_SPEC.md) includes the 28 user stories and testing
decisions; [ADR 0001](adr/0001-preserve-stream-baseline-during-input-latency-work.md)
records the agreed scope. The diagnostic package and verified rollback remain
the same artifacts as before this documentation work.

The [first hardware analysis](LATENCY_BASELINE_RESULT.md) justifies software
preparation. Remaining baseline acceptance directly gates hardware comparisons
and adoption; it no longer blocks analysis or implementation preparation.

The [first hardware analysis](LATENCY_BASELINE_RESULT.md) records local timing
findings and remaining uncertainty. Its useful partial evidence does not mark
the full diagnostic gate or the later candidate tasks complete.
