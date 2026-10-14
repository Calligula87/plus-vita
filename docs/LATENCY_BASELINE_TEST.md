# Input latency: diagnostic baseline

Planning: [canonical spec #1](https://github.com/Calligula87/plus-vita/issues/1),
[next hardware task #3](https://github.com/Calligula87/plus-vita/issues/3),
[complete test plan](LATENCY_TEST_PLAN.md), [task index](LATENCY_TASKS.md), and
[ADR 0001](adr/0001-preserve-stream-baseline-during-input-latency-work.md).

Build: `live-720p-refs5-latency-baseline-1`.

Packaged artifact: `build/debug/plus-vita-latency-baseline-1.vpk`, 1674090
bytes, SHA256
`B48C54FDF8616512CCB755E4502819BB10EEF8E1EEC7DEEA6FC8B92BEAF1EC23`.

This is the first step of the agreed latency investigation: preserve picture
quality and streaming stability, measure the accepted video path, then evaluate
independent input sampling. This package instruments the stable reorder build;
it does not implement the input worker or claim improved response time.

## Preserved baseline

The accepted VPK remains `build/debug/plus-vita-video-reorder-1.vpk` (1670238
bytes, SHA256
`6F4FEB9A3FAC9F90BC201AF0110DAA28979DC130E321F7CBE5F8BFA0D313E56B`).
The rollback readback is also preserved at
`build/debug/plus-vita-stable-rollback-readback-20261013.vpk` with the same hash.

The new package retains video allocation/profile selection, five-slot decoder
guards, FEC, the three-slot/20ms reorder policy, synchronous two-buffer
presentation and catch-up policy, accepted audio, controller mappings and stop
chord. Recording remains off. The feedback sender remains event-driven with
its existing idle keepalive. No upstream submodule files are edited.

The failed display-worker experiment remains deferred; see
[DISPLAY_PACING_TEST.md](DISPLAY_PACING_TEST.md).

## Measurements

Fixed-size histograms collect durations in memory. New timing summaries are
written to the existing `ux0:data/plus-vita-allocation.log` after receive and
feedback workers have stopped. No extra per-frame or per-button log lines are
added. Existing diagnostic writes and flushes are retained and timed, so this
baseline does not also change logging behavior.

| Metric | Interval and interpretation |
| --- | --- |
| `input_poll_gap` | Start of one control poll to start of the next, including failed polls. Reveals how long video work or scheduling prevents the next poll. It excludes time before the first poll and after the last. |
| `input_publish_call` | Duration of the changed-state setter, including its mutex and signal. Does not wait for the network send. |
| `feedback.sample_to_publish` | Beginning of the Vita poll through controller/touch mapping to changed-state publication under the sender mutex. |
| `feedback.publish_to_wake` | Publication to sender consumption; separate from sampling and transmission. |
| `feedback.sample_to_state_send`, `feedback.sample_to_history_send` | Poll start to successful return from the local Takion send API. State packets carry sticks; history packets carry buttons/triggers/touch. Excludes idle resends and synthetic neutral updates. Multiple history packets from one update produce multiple samples. |
| `feedback.state_send_call`, `feedback.history_send_call` | Local send API duration, including failed attempts and idle state resends. Successful return does not establish receipt by the server. |
| `main_mutex_wait`, `callback_mutex_wait` | Main preview reacquisition / receiver callback acquisition of the probe mutex. Main condition-variable reacquisition is not included. |
| `callback_work` | Receiver callback work while holding that mutex in the media state, including decrypt/assembly/FEC and existing logging. Excludes socket receive/parsing before callback entry. |
| `frame_assembly` | First admitted packet's callback entry to complete source/FEC assembly. Includes local waits and intra-frame arrivals, but excludes transport before the first packet. |
| `ready_to_enqueue` | Complete assembly to compressed queue insertion, including any predecessor hold, validation and existing logging. Accepted queued frames only. |
| `queue_wait` | Queue insertion to dequeue for decoding. Queue capacity is not a fixed delay. |
| `decode_prepare`, `decode` | Decoder preparation and hardware decode call, using the existing timing boundaries. |
| `display_wait` | Existing synchronous framebuffer wait call. |
| `submitted_first_packet_to_present`, `submitted_ready_to_present` | Boundary of the currently submitted AU to successful presentation function return when that call produced an output and it was presented. Skipped decoded updates are excluded. Native output can belong to an earlier AU, so these are submission intervals, **not the displayed picture's age**. |
| `log_write_call`, `log_mutex_wait` | Existing diagnostic formatting/write/flush under its mutex, and time waiting for that mutex, during the preview collection period. Shutdown summaries are excluded. |

Each metric reports count, mean, maximum and estimated p50/p95/p99 **bucket
upper bounds**, in microseconds. The bounds are capped by the observed maximum;
they are not exact percentiles. Backward clock spans are rejected and counted
as `invalid`, rather than becoming enormous unsigned durations. No samples is
reported as count=0, not evidence of zero latency.

The feedback summary reports publications, consumed updates, coalesced pending
updates and failed local sends. The existing latest-state behavior can combine
multiple publications before a sender wake; timings follow the latest consumed
sample, not each physical press. These counters make that limitation visible.
Timing metadata uses the existing sender mutex and is copied only after join;
the adaptation supports the client's single active sender. Timing metadata and
histograms require approximately 6 KiB of fixed storage across probe, sender and logger.
Clock reads and histogram updates have some overhead; hardware testing must
check whether the instrumented baseline still behaves like the stable package.

These measurements do **not** establish button-to-game-response latency. Cloud
transport, server input processing, game frame production, and physical scanout
are outside the measured local intervals. No input values or packet payloads
are added to the log by this instrumentation.

The native decoder is an additional uncertainty: the audited replay retained
three pictures for final draining ([720P_DECODER_PLAN.md](720P_DECODER_PLAN.md)).
Input/output ordering without B-frames does not prove zero decoder buffering.
The stable decoder does not provide a per-AU output timestamp, so the two
submission-to-presentation intervals omit any age accumulated by the older
output picture. Existing submitted/decoded counters help identify no-output
calls but do not establish an exact live buffer depth after losses or recovery.
The diagnostic build leaves decoder parameters and timestamps unchanged.

## Vita test

1. Use the same game, scene, Wi-Fi location and streaming settings as the
   accepted build. Save any existing allocation log before launching again:
   each launch replaces it.
2. Copy `build/debug/plus-vita-latency-baseline-1.vpk` to the Vita and **install**
   it in VitaShell. Uploading alone does not update the installed executable.
3. Use a repeatable visible response, such as moving a menu selection with the
   D-pad. Make ten separated presses. Also play normally, including analog
   movement and the existing touch mappings, for 8–10 minutes. Note freezes,
   stutter, missing controls and audio changes.
4. Stop by holding START + SELECT for two seconds, or let the ten-minute cap
   finish. A clean stop is needed for the timing summary; a crash may leave
   only the existing diagnostic lines.
5. Retrieve `ux0:data/plus-vita-allocation.log` before the next launch. Check
   `Stream probe build=live-720p-refs5-latency-baseline-1`, the latency summary,
   clean shutdown, and the normal queue/recovery/FEC/decode/audio counters.
   Store the result locally under ignored `build/debug/`.

If the diagnostic build behaves worse, reinstall the preserved reorder VPK
and investigate instrumentation overhead before proceeding.

### Optional phone measurement

Film the button and screen together at 120 or 240 fps, using the same ten
presses and visible response on both builds. For each press, estimate
`(first response frame - press frame) * 1000 / recording fps` milliseconds.
Use the median and range, and keep the press/response definitions consistent.
One recording frame represents about 8.3ms at 120 fps or 4.2ms at 240 fps;
physical contact and screen scanout add uncertainty. Use the actual recording
frame rate, not a slowed playback frame rate.

## Comparison and next change

Review polling gaps, sender handoff, queue age, display waits and log stalls
alongside stability counters. Then evaluate a separate build with independent
input sampling and the same instrumentation. Keep video/audio/mappings
unchanged in that experiment and compare the same scene and test duration.
Retain it only after a repeatable responsiveness improvement and an 8–10 minute
run without new stability or control regressions. Do not infer improvement
from host tests or fewer dropped frames alone.

Source references inspected for this approach:

- [Vitaki input thread and deadline pacing](https://github.com/mauricio-gg/vitaki-vitarps5/blob/468638e70a8899b000196d5d73b1d35e221ba968/vita/src/host_input.c#L651-L668).
- [GreenVita input polling during pacing](https://github.com/Day-OS/green-vita/blob/ae2625d295b4fba005a769b1309fd70dcd6cb63f/src/shell/mod.rs#L242-L266).
- [Pylux event-driven feedback sender](https://github.com/ForWard-Technologies-LLC/Pylux/blob/ca87a49a12bab071948f565ca53ffdad790775d3/lib/src/feedbacksender.c#L348-L398).
- [OpenNOW timing counters](https://github.com/OpenCloudGaming/OpenNOW-vita/blob/654a9356a9f01df4a67ea802b8f8c96af7ea2ca3/src/gfn/peer.rs#L912-L986). Its presentation mailbox is a reference for later investigation, not evidence that our deferred display-worker change is safe.

## Validation

After configuring/building the Vita target, run `sh tools/test-latency.sh` in
the existing Ubuntu WSL environment. It compiles the **actual generated** sender
used by the VPK and an upstream sender, drives identical control changes, and
compares their feedback output. AddressSanitizer/UndefinedBehaviorSanitizer
checks cover histogram boundaries and a stall fixture, million-sample totals,
backward clocks, concurrent log writers, summary exclusion/reset/redaction,
send failures, input coalescing and sender session restart. To check thread
ownership, run `PLUS_VITA_TEST_SANITIZERS=thread sh tools/test-latency.sh`.

VitaSDK packaging and host checks establish build and instrumentation behavior;
the diagnostic baseline's performance and stability remain pending hardware
validation.

On 2026-10-14, Release compilation/VPK packaging, the complete
AddressSanitizer/UndefinedBehaviorSanitizer test script and the complete
ThreadSanitizer test script passed. Feedback output matched the upstream
sender for the exercised control sequence. The preserved stable VPK's SHA256
was rechecked and matches the accepted rollback package. At that point the
package had not been uploaded or installed on the Vita.

## Deployment, 2026-10-14

With the user's FTP authorization, downloaded the existing
`ux0:data/plus_vita_smoke.vpk` to
`build/debug/plus-vita-before-latency-baseline-1-remote-20261014.vpk` before
uploading. Its SHA256 matched the accepted reorder package listed above.

Uploaded the diagnostic baseline to `ux0:data/plus_vita_smoke.vpk` and downloaded
it back to `build/debug/plus-vita-latency-baseline-1-readback-20261014.vpk`.
Readback matched 1674090 bytes and SHA256
`B48C54FDF8616512CCB755E4502819BB10EEF8E1EEC7DEEA6FC8B92BEAF1EC23`.
Only that remote VPK was replaced. Installation in VitaShell and the hardware
baseline remain pending; uploading does not update the installed application.

## First hardware result, 2026-10-14

The retrieved log verifies the diagnostic build ran for 352645ms before clean
user stop. All 21 metrics are present with zero invalid spans. No native video,
display, input-send or audio-output failure was found. See the
[analysis report](LATENCY_BASELINE_RESULT.md) for the timing distributions,
logging cost, video recovery events, existing audio backlog and limitations.
The first run is shorter than the planned 8–10 minutes; subjective quality and
a matched response comparison are not established by the log. Full acceptance
and the independent-input candidate remain pending.
