# Latency work: task index

Canonical specification: [#1](https://github.com/Calligula87/plus-vita/issues/1).
Tasks are GitHub sub-issues with native dependencies; this is a local status
snapshot dated 2026-10-14, not a second issue tracker.

Ready software work, in priority order: **#5 input isolation**, **#8 deferred
logging as a separate candidate**, and **#9 audio backlog investigation**.
Ready means the work can start; it does not mean it is running or accepted.

| Task | State after results triage | Dependency / owner |
| --- | --- | --- |
| [#2 Deliver diagnostic baseline and software validation](https://github.com/Calligula87/plus-vita/issues/2) | Completed locally; issue closed | Agent/software delivery only. Source uncommitted; no hardware acceptance. |
| [#3 Complete remaining diagnostic baseline checks](https://github.com/Calligula87/plus-vita/issues/3) | Open; first clean 5m52.645s run inspected | Full duration/subjective/restart gate remains; `ready-for-human`. |
| [#4 Analyze baseline delays and select the input experiment](https://github.com/Calligula87/plus-vita/issues/4) | Analysis completed; issue closed | Available evidence analyzed, not full acceptance; `ready-for-agent`. |
| [#5 Isolate input sampling and validate its lifecycle](https://github.com/Calligula87/plus-vita/issues/5) | Ready to implement, unblocked | Completed #4; `ready-for-agent`. |
| [#6 Compare independent input sampling on Vita](https://github.com/Calligula87/plus-vita/issues/6) | Open | Blocked by #3 and #5; `ready-for-human`. |
| [#7 Record candidate acceptance or rollback](https://github.com/Calligula87/plus-vita/issues/7) | Open | Blocked by #6; `ready-for-agent`. |
| [#8 Defer diagnostic file I/O outside streaming hot paths](https://github.com/Calligula87/plus-vita/issues/8) | Ready to implement, unblocked | Logging-only candidate; `ready-for-agent`. |
| [#9 Investigate existing audio queue backlog](https://github.com/Calligula87/plus-vita/issues/9) | Ready to investigate, unblocked | No playback-policy changes; `ready-for-agent`. |
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
