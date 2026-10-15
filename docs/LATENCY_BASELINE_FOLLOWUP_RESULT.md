# Diagnostic baseline follow-up

2026-10-15. Correct build: `live-720p-refs5-latency-baseline-1`, mode `baseline`.
Related [baseline gate #3](https://github.com/Calligula87/plus-vita/issues/3)
and [input comparison #6](https://github.com/Calligula87/plus-vita/issues/6).

## Evidence and acceptance

Retrieved the user-authorized FTP log into private ignored local artifact
`build/debug/plus-vita-baseline-followup-20261015-222442.log`, 356955 bytes,
SHA256 `269A63E65FC87BDA2F703964C5BE344C1DF61F9B934DA6E30B0CC8524C7F90D5`.
Structured aggregates are retained in
`build/debug/plus-vita-baseline-followup-analysis-20261015.json`.
No package, configuration or remote log was modified during retrieval.

The preview ran **466418ms (7m46.418s)** before clean user stop. Preview result,
display restoration, disconnect send, stream completion and allocation
completion all succeeded. Audio drain/release succeeded. All 21 timing metrics
are present, with zero invalid spans, input poll failures or observed local
input-send failures. The log contains one completed allocation and does not
establish subsequent-session success or perceived control/quality equivalence.

This is about 13.6 seconds shorter than the planned eight-minute minimum. The
maintainer explicitly accepted this duration for the baseline, satisfying the
duration criterion without another long run. The maintainer also reported
front touch was not tested and rear L2/R2/L3/R3 behavior was uncertain.
Earlier subjective quality/stability equivalence was already reported, but
specific rear/front mappings/releases, ten response actions and subsequent
session success remain unconfirmed. **#3 remains open for these human checks;
candidate hardware acceptance is not inferred.** The saved log can now be
safely followed by a short baseline control/restart check without losing it.

Before live streaming, five error-level entries were recorded: two Senkusha
pong receive timeouts with their paired receive-error reports, and one Takion
data-ack size rejection. The later live session reached clean completion.
These are retained as recovered pre-stream diagnostics; this report does not
claim the entire log is free of error-level messages or establish their cause.

## Local timing comparison

Milliseconds; percentile values are conservative histogram upper bounds.
Intervals overlap and must not be added into a response-latency estimate.

| Metric | Earlier baseline mean / max | Follow-up mean / max | Follow-up p95 upper |
| --- | ---: | ---: | ---: |
| Input poll interval | 15.812 / 57.513 | 15.987 / 102.778 | 25 |
| Poll start to state-send return | 0.357 / 1.065 | 0.368 / 1.833 | 1 |
| Poll start to history-send return | 0.427 / 0.815 | 0.398 / 0.908 | 0.908 |
| Compressed queue residence | 14.222 / 88.925 | 15.337 / 89.337 | 33 |
| Hardware decode call | 6.909 / 14.233 | 6.899 / 17.124 | 8 |
| Framebuffer wait | 9.187 / 16.671 | 9.231 / 16.712 | 12 |
| Synchronous diagnostic write/flush | 10.565 / 35.083 | 10.683 / 70.519 | 20 |

5529 changed input updates were published and consumed, with zero coalescing.
5324 state-send timing samples and 255 history-send samples are associated with
sampled changes. These counts do not represent physical presses or remote
receipt. Input samples still reach local sending quickly once polled; the
~16ms average poll interval and 103ms tail remain relevant to the isolated
input experiment. They do not establish the dominant whole-response cause.

2917 measured synchronous writes account for approximately **31.162s**, or
**6.68%** of preview elapsed time, using rounded means. This is aggregate
serialized file work, not CPU utilization or continuous freezing. Main-lock,
callback-lock and callback-work maxima were 69.403, 66.828 and 71.648ms; those
overlapping tails are not assigned to disk solely from this log.

## Streaming continuity and audio

- 27759 video submissions, 27752 native outputs and 25280 displayed updates:
  about 54.20 displayed updates/s. 2472 decoded updates were replaced (~8.91%
  of native outputs); this is not compressed-reference loss.
- Queue peak 6/16, no overflow stop. 380 successful FEC repairs, zero failures,
  about 48.88 repairs/minute; maximum repair 2.329ms. The earlier baseline had
  ~75.88 repairs/minute. Different loss conditions prevent causal attribution.
- Three incomplete-frame recoveries resumed after 157, 86 and 159ms;
  31 dependent units discarded in
  total, no recovery remained pending. Reorder pressure drops and wholly
  missing-frame retirements were zero. Total blocked reorder wait 510.032ms,
  maximum 31.164ms, across the whole run; grace polling can exceed 20ms.
- Audio queue residence averaged **246.675ms**, maximum **315.514ms**, with 11
  local evictions, 19 concealed units and 202 stale dequeues. Output failures
  and sequence resets were zero. The 48 output-gap stalls and zero output-call
  stalls retain the previously observed distinction between gaps and blocked
  device calls. This pre-existing backlog remains a separate investigation.

The follow-up broadly repeats the earlier local timing pattern and has longer
worst-case scheduling/write tails. It validates another clean baseline session,
not an improvement. No numerical physical input-to-screen measurement, matched
native output age or definitive audio-backlog cause is available.

After #3 is explicitly completed, install/test **C first**:
`ux0:data/plus-vita-input-worker-1.vpk`, build
`live-720p-refs5-input-worker-1`. Compare repeated actions with B and run the
control/stability procedure in [candidate test steps](LATENCY_CANDIDATE_TEST.md).
Retrieve each log before the next allocation. Test logging-only L separately
later, and leave accepted rollback S available.

Remaining short baseline check: rear upper-left = L2, upper-right = R2,
lower-left = L3, lower-right = R3. On the front screen, one contact supplies
touchpad position/movement and two simultaneous contacts press touchpad click.
Verify releases as well as presses in a scene that visibly uses those actions,
repeat ten separated response actions if not already done, then cleanly stop
and start another stream. Report unavailable game-specific actions as unknown,
not as passed or failed. No repeat long session is required by the accepted
duration revision.
