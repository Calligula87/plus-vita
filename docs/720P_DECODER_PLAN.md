# Agreed 720p decoder plan

User approved this plan on 2026-09-26 and explicitly requested that work not
deviate from it. Read this document before continuing decoder experiments.

## Objective

Determine whether the existing 1280x720 cloud H.264 stream can be decoded
correctly on Vita. Nine references/buffered pictures are declared in the SPS;
actual reference-picture usage has not yet been established.

## Sequence and gates

1. Capture a short complete H.264 sample from the existing decrypted video
   pipeline. Start at an IDR with the matching unmodified SPS/PPS. Bound storage
   and duration, record gaps, and exclude credentials and transport secrets.
   Do not change allocation, authentication, profile negotiation, or SPS values.
2. Analyse reference-picture usage and buffering requirements offline. Include
   PPS, slice reference lists, reference marking/MMCO, long-term references and
   output reordering. An incomplete sample is not evidence for a lower limit;
   a short sample cannot establish a universal bound for all future sessions.
3. Only if evidence supports the five-slot native configuration, implement a
   controlled, validated SPS adjustment experiment. Keep original bytes and
   fail closed on unsupported syntax or requirements exceeding the limit.
   Do not blindly copy Moonlight/GreenVita's one-reference setting.
4. If nine slots are genuinely required, stop lowering declarations. Present
   software decoding and an external transcoding relay as alternatives with
   measured performance/tradeoffs; obtain direction before expanding scope.

## Evidence already established

- Native initialization rejects six through nine slots at 1280x720 on the
  tested device. Five initializes but sustained unmodified decode fails.
- GreenVita uses 1280x720 input, 960x544 output and one native reference slot;
  it is not proof of nine-slot support.
- Moonlight rewrites SPS buffering metadata for its own stream assumptions;
  this is a precedent to investigate, not a safe drop-in patch.

Sources:
- https://github.com/Day-OS/green-vita/blob/master/src/streaming/video/decoder.rs
- https://github.com/Day-OS/green-vita/blob/master/src/streaming/video/mod.rs
- https://github.com/xyzz/vita-moonlight/blob/vita/libgamestream/sps.c

## Current status

Step 1 implemented as capture-720p-1: select the matching advertised 720p
header locally without changing negotiation, bypass native decoding, wait for
an Annex-B IDR, retain at most 8 MiB/300 access units/5 seconds after IDR,
and save video-only bytes after closing transport. Overall wait is 15 seconds.
Detected gaps/profile switches stop collection and label the sample partial.
Incomplete frames before the initial IDR are logged but do not invalidate
the new IDR sequence. No disk writes occur in the receive callback.
Each run logs its uniquely named ux0:data/plus-vita-capture-*.h264 file.
Capture completion is not a playback or reference-count compatibility result.
VitaSDK build and VPK packaging succeeded. Uploaded to
192.168.1.15:1337/ux0:/data/plus_vita_smoke.vpk and downloaded back;
SHA256 matched 206BD78B907FFCDC4A4041032CE39BA71A74A936CAA84D9180F15ABE3FD20764.
Previous remote package backed up locally as
build/debug/plus-vita-before-capture-720p-1.vpk. Credential file was left untouched.
Capture retrieved: build/debug/plus-vita-capture-162454927-complete.h264,
2,722,255 bytes, SHA256
393142853d1a0679daa7964589f3c8c41e8bd96578e8bf7430ed6107920a7d5c.
Correct build, capture-only 720p, completion err=0. Capture-specific messages
were omitted by the diagnostic prefix allowlist; their prefix is now corrected.

## Step 2 result and bounded step 3 experiment

FFmpeg 9.0.2 strict software decode accepts all 300 pictures. Header tracing
finds 3 IDR slices and 2415 P slices; every P slice uses one default L0 entry,
no list modifications, no active-count overrides, no adaptive marking/MMCO,
and no long-term reference marking. Progressive frames, POC type 2, no
B-frames/reordering, and frame numbers are contiguous modulo 128.
The original sliding-window DPB can retain nine pictures, but these slices
can select only its newest short-term picture. This is sample-specific
evidence, not a limit established for future live streams.

tools/audit-720p-capture.mjs fails closed on syntax outside these conditions.
It changes ONLY max_num_ref_frames and max_dec_frame_buffering from 9 to 5,
leaves profile/level/resolution untouched, rebuilds RBSP alignment/emulation
prevention, and compares software-decoded framemd5 for every picture.
All 300 original/patched frame hashes match exactly. Original file retained.
Audit report: build/debug/plus-vita-replay-720p-audit.json.

replay-720p-refs5-1 is a bounded hardware replay test, not a live SPS patch.
It accepts only the exact SHA256-audited replay container, validates every
AU bound before initializing resources, and submits all 300 AUs in order.
It fails if any native call fails, outputs are missing, or 15 seconds elapse.
No new cloud allocation is requested for replay; existing live allocation
configuration and credential persistence are unchanged. Any selected title
starts the same recorded sample. It displays at 960x544 from 1280x720 input.

VitaSDK build succeeded. VPK and replay data uploaded to ux0:data on
192.168.1.15:1337. Previous VPK saved locally as
build/debug/plus-vita-before-replay-720p-refs5-1.vpk.
VPK SHA256: A6741C48B6EDF227B3C4C16D02FFC8AE0F92F4328E78228C62C02BF9AC7EF26C.
Replay SHA256: ea735a32bcc166a8d375830dbebddfff08c58db47fad7b47c6453b043786b802.
Awaiting installation and hardware replay result. Do not enable this rewrite
for arbitrary live streams without a runtime syntax/reference guard and
further evidence. Retain the existing valid NPSSO credential file.

2026-09-27: refs5-1 hardware log submitted all 300 AUs with native result zero,
displayed 297 pictures, and failed the local count check. The first three
submissions produced no output. Replay lacked end-of-stream draining; buffered
tail pictures are a hypothesis to verify, not yet a confirmed 300-frame pass.
refs5-2-drain now calls sceAvcdecDecodeStop with one output slot repeatedly,
bounded to 17 calls and the original 15-second deadline. Each call is logged;
pass requires exactly 300 displayed pictures and a final zero-output call.
The stop ABI was cross-checked against Vita3K SceVideodecUser.cpp; actual
hardware drain behavior remains to be tested. Live streaming remains unchanged.
Build succeeded; replaced ux0:data/plus_vita_smoke.vpk at 192.168.1.15:1337.
Readback SHA256 matches:
9D914FDC016B53C987E982A9F93B489DB257FEDA90B575FD31845C9748755B1B.
Old package backed up as build/debug/plus-vita-before-replay-refs5-2-drain.vpk.
NPSSO file untouched. Awaiting hardware test.

Hardware result retrieved 2026-09-27:
build/debug/plus-vita-replay-refs5-2-drain-result.log confirms the correct build,
300 submitted / 300 displayed, native result 0, replay.complete code 0.
Three successful drain calls each returned one picture, followed by zero output.
This validates end-of-stream draining and the bounded audited sample on native
720p/five-slot decoding. It does not validate arbitrary live SPS rewriting or
Vita output pixel hashes. Next gate: a fail-closed runtime syntax/reference guard
before a bounded live playback experiment; keep the original capture and audit.

Next implementation: live-720p-refs5-guard-1, ten-second live video preview.
Narrow guard accepts ONLY exact original audited SPS/PPS bytes and substitutes
the host-verified patched SPS. Every VCL slice must match the audited P/I
syntax, PPS zero, seven-bit contiguous frame_num, increasing slice macroblock
addresses, no active-ref override/list modification/adaptive marking or long-term
IDR references. Starts at IDR; rejects later IDRs, unsupported/in-band headers,
profile switches after start, and frame gaps. Transport parser disagreement
also stops before decode. This intentionally sacrifices compatibility for a
bounded first experiment; rejection is not proof native decode cannot work.
Before live transport, checks SHA256 of the retained replay container and runs
the C guard on all 300 AUs plus duplicate/truncated negatives and header
mutation/truncation tests. Tests execute on-device; not claimed passed yet.
Build succeeds; source diff whitespace checks clean. Embedded SPS arrays match
the original/patched audited files exactly. No host C compiler is available.
Live preview uses existing queue/display/thread handling; audio is received
but not played. Authentication and negotiation unchanged. NPSSO untouched.
Uploaded to ux0:data/plus_vita_smoke.vpk at 192.168.1.15:1337; readback verified.
Previous replay VPK retained as build/debug/plus-vita-before-live-guard-1.vpk.
Awaiting on-device guard self-test and live preview logs.

guard-1 live log retrieved: build/debug/plus-vita-live-guard-1-result.log.
Guard self-tests passed; exact header accepted; native decoder initialized.
59 submitted / 54 decoded / 25 displayed, no native codec error. Stopped on
generic slice/reference guard rejection with one incomplete frame recorded.
Cannot distinguish repeated IDR, frame_num gap, or reference changes from that
log. Do not infer nine references required or relax the guard from this evidence.

live-720p-refs5-guard-2-diag adds named rejection reasons and observed/previous
frame_num, NAL/slice/MB, wire frame and incomplete count. Captures bounded
unmodified video from IDR (8 MiB/300 AUs/5 seconds, gap-labelled), and separately
retains the rejected AU (at most 1 MiB) with ORIGINAL SPS/PPS. Writes only after
transport closes, video-only, no credentials or transport payloads. Slice guard
acceptance unchanged; this is diagnostic instrumentation, not a claimed playback
fix. Existing 300-AU and negative guard tests still run before live transport.
Build and whitespace checks succeeded; uploaded/readback SHA256 matches:
9D86DEF12E8429E0309C4E10515FC1466FD4DDF1C60C85A71C6A4AFCDB760A12.
Old package backed up as build/debug/plus-vita-before-live-guard-2-diag.vpk.
NPSSO untouched. Awaiting test, then retrieve log and named .h264 diagnostics
for targeted offline header/reference analysis before changing acceptance rules.

guard-2 diagnostic result retrieved: 333 submitted / 326 decoded / 273 displayed,
no native codec errors, no incomplete/stale frames. Rejected wire frame 334 with
reason ADDITIONAL_IDR, previous frame_num 76, observed 0, NAL 5, slice 7, MB 0.
Retrieved plus-vita-rejected-97604406.h264 (267 bytes including original headers)
and plus-vita-capture-97735524-partial.h264. FFmpeg trace independently confirms
IDR frame_num 0, PPS 0, idr_pic_id 1, no_output_of_prior_pics_flag 0,
long_term_reference_flag 0. This stop is the deliberately narrow repeated-IDR
rule, not packet loss or native decoder failure. Earlier guard-1 generic failure
remains unclassified; do not assume the same cause without its rejected data.
Next implementation should admit a fully validated new short-term IDR as a
reference reset, while retaining every other syntax/reference check. Update
duplicate-IDR negative tests: repeated IDRs cannot be distinguished from valid
resets solely by frame_num, so transport wire-frame continuity must protect
against duplicate access units. Verify decoder behavior across the reset.

live-720p-refs5-guard-3-idr implements validated repeated short-term IDRs:
all slices must agree on idr_pic_id, frame_num must be zero, long-term flag
must remain zero. Guard state commits only after the full AU passes. Other
checks unchanged. Added independent sequential 16-bit wire-frame requirement
after the first accepted IDR; existing stale/duplicate packet filtering remains.
Accepted IDR resets are counted/logged. On-device self-tests now accept an IDR
reset, reject duplicate P frames and long-term IDR, and check wire wraparound,
duplicates, gaps and reverse order. Native behavior across resets still awaits
hardware verification. Build succeeded; prior VPK backed up locally as
build/debug/plus-vita-before-live-guard-3-idr.vpk. Uploaded to existing data VPK
path and verified by readback. NPSSO unchanged. Expected bounded ten-second
live preview should now cross ordinary keyframe resets without guard failure;
any other rejection remains diagnostic, not a reason to remove safety checks.

guard-3 result: build/debug/plus-vita-live-guard-3-idr-result.log.
Stopped on WIRE_FRAME_GAP previous=192 observed=194, with one incomplete
frame and four stale packets. 192 submitted / 187 decoded / 145 displayed,
no native codec error. Only initial IDR observed, so repeated-IDR decoder
behavior remains unverified. This is a distinct packet/frame assembly gap,
not the previous ADDITIONAL_IDR stop. Next design should recover missing
compressed frames using existing protocol FEC/recovery where possible; if a
reference AU is still missing, stop feeding dependent P frames and request/wait
for a fully validated IDR to resume. Do not merely bypass frame continuity.

live-720p-refs5-guard-4-recovery implements bounded safe IDR recovery fallback,
not FEC reconstruction. On missing wire generations, reports CORRUPTFRAME on
reliable channel 2 using upstream streamconnection.c's message/channel layout.
Stops feeding dependent P frames; retains the valid pre-gap decode queue in
order, waits at most five seconds, and resumes only on a fully validated IDR.
Skipped dependent frames advance transport stale-filter bookkeeping, not H.264
reference state. New IDR resets reference state, includes the same audited SPS,
then strict continuity resumes. Other guard rejections remain fatal. Counts,
request result, wait time and resume wire frame logged. Overall preview bounded
to 25 seconds from start; no success while waiting for recovery. Capture/auth/
allocation settings unchanged. Added self-test short-term IDR reset after a
rejected long-term IDR; retained audited sample tests. Build succeeds; hardware
recovery behavior awaits test. Previous VPK backed up as
build/debug/plus-vita-before-live-guard-4-recovery.vpk. Uploaded/readback SHA256:
570ADFD29EAFA334656835898750A9A9D7AFAF9687939BE530AAB8D7C75FF0A9.
NPSSO untouched. Expected brief freeze on loss, then validated-IDR resume,
or explicit IDR_TIMEOUT if server does not provide one within the bound.

guard-4 hardware run PASSED: build/debug/plus-vita-live-guard-4-recovery-result.log.
Ten-second live preview: 608 submitted, 600 decoded, 538 displayed, 62 decoded
presentation updates skipped to catch up. Two validated IDRs, including reset
at wire frame 322; playback continued across it. Native decode errors none,
stream_probe.complete code 0, display restored and transport cleanly closed.
No incomplete/stale frames or recovery episodes in this run: IDR-reset fix is
verified, but packet-loss recovery itself has NOT been exercised/verified.
Native decode total 4,161,873us, max 19,839us; not a guarantee of sustained60fps.
Remain narrow audited syntax/five-slot guard; audio playback/input functionality
and longer sessions still pending. Preserve this working build as baseline.

Next gate implemented: live-720p-refs5-guard-5-loss-test. Thirty-second preview,
one deliberate local post-IDR P-AU omission after 120 accepted frames, before
reference/wire state advances. Following generation exercises real CORRUPTFRAME
and wait-for-validated-IDR path. Pass requires injection and at least one resume,
no pending recovery, recent displayed output at end, existing audio reception.
Five-second recovery deadline unchanged; overall preview bound now 45 seconds.
Long-term refs and other unsupported syntax remain rejected. Captures across
intentional omission labelled partial. No audio playback or new decoder profile.
Working guard-4 VPK preserved as build/debug/plus-vita-working-guard-4-baseline.vpk.
Build and source whitespace checks succeeded. Uploaded and readback verified:
049D4EA659CBDE027E9A9085E7B0B3804506E7A0E332E1DDAE666E1FAD8C0E96.
NPSSO unchanged. Awaiting hardware controlled-loss result; not yet a recovery
pass. Disable deliberate fault injection after validating this gate.

guard-5 failed controlled-loss test: log
build/debug/plus-vita-live-guard-5-loss-test-result.log. Deliberate drop wire121,
CORRUPTFRAME sent successfully; valid IDR arrived wire317 after3232ms. Recovery
cleared awaiting_idr, then main thread immediately applied old last_display
three-second deadline and terminated BEFORE submitting recovery IDR. Native
decoder was not the cause; 120 submitted /114 decoded /76 displayed.

live-720p-refs5-guard-6-recovery-deadline fixes stale deadline: keeps actual
last_display accurate, tracks validated recovery resume time separately, uses
the later timestamp for no-output timeout. Queued IDR carries recovery generation;
pending recovery output can only clear when a display update occurs after that
generation is submitted. Pending output defers preview success, and is forced
to display rather than presentation-skipped. Five-second IDR wait, three-second
post-IDR output deadline, 45-second overall bound remain. Thirty-second controlled
loss test remains enabled to verify the fix, not yet claimed passed.
Build/whitespace checks succeeded; previous test backed up as
build/debug/plus-vita-before-live-guard-6-deadline.vpk. Uploaded and readback
verified. Working guard-4 baseline retained; NPSSO unchanged.

guard-6 hardware controlled-loss run PASSED: log
build/debug/plus-vita-live-guard-6-deadline-result.log. Thirty-second preview,
1401 decoded /1261 displayed, final stream_probe.complete code 0, no native
decode errors and clean transport/display shutdown. Deliberate omission wire121
recovered at validated IDR293 after2833ms, displayed output22ms after IDR.
A naturally missing wire1472 also recovered at IDR1697 after3743ms, displayed
output24ms after IDR. Two recovery episodes/resumes, no pending recovery at end.
This verifies bounded safe recovery, not seamless loss concealment or FEC;
roughly three-second freezes remain. Audio received only, not played.
Next gate: disable deliberate fault injection and preserve this working recovery
baseline before adding functionality. No new build/upload performed for log review.

Next gate deployed: live-720p-refs5-guard-7-normal-playback. Deliberate fault
injection disabled; thirty-second duration and verified recovery/decoder guards
unchanged. UI now describes normal playback. Build succeeded. Working guard-6
preserved as build/debug/plus-vita-working-guard-6-baseline.vpk; remote old VPK
backed up as build/debug/plus-vita-before-guard-7-remote.vpk. Replaced the same
ux0:data/plus_vita_smoke.vpk at 192.168.1.15:1337, readback SHA256 matches:
92F96CD2830A2C260D36D6701665D8C6EDD6E9EC707973F0E2CF7BEB6A352727.
NPSSO untouched. Awaiting hardware normal-playback result; audio playback still
not enabled. Expected thirty-second preview without an intentional freeze;
natural packet loss can still cause bounded wait-for-IDR recovery.

guard-7 normal playback PASSED: build/debug/plus-vita-guard-7-result.log.
Thirty seconds, 1806 submitted /1796 decoded /1628 displayed, no incomplete
or stale frames, no recovery episodes, four validated IDRs, final code0 and
clean shutdown. Intentional fault injection confirmed disabled.
Next stability gate guard-8 changes only preview duration to60seconds plus
build/UI labels; decoder and recovery unchanged. Working guard-7 backed up as
build/debug/plus-vita-working-guard-7-baseline.vpk. Build succeeded; uploaded
same remote VPK and verified readback SHA256:
3F08F4524405773EE69C5D6EB9EFB95D511EB436FDC20EFE04AAB990CD9344A6.
NPSSO untouched. Awaiting hardware result; audio playback/controls pending.
Repository-wide whitespace check reports pre-existing CMakeLists.txt line292
trailing whitespace, unrelated to this duration-only change.

guard-8 hardware stability test FAILED: build/debug/plus-vita-guard-8-result.log.
Correct build and fault injection disabled. Three natural loss episodes recovered
after1946ms,203ms,92ms. Fourth gap previous1924/observed1926 sent CORRUPTFRAME
successfully but no validated IDR within5000ms; stopped safely with IDR_TIMEOUT
and finalcode15, before60seconds. 1780 decoded /1592 displayed, seven incomplete
frames,25stale packets; no native decoder error, display restore successful.
This identifies loss/recovery availability as remaining stability blocker, not
native decode failure. A successful send does not prove server handled request.
Next diagnostic design: investigate request retry/acknowledgement and existing
upstream FEC assembly before changing recovery behavior; do not bypass guards
or simply raise timeout. No code/build/upload change during this log review.

guard-9 recovery-reports targets suppressed corruption reporting while awaiting
IDR: guard-8 sent only the initial missing range; upstream videoreceiver reports
subsequent missing/corrupt frames. While dependent AUs are discarded, re-report
unusable range from original missing frame through current AU every500ms, only
within existing5000ms deadline (at most ten sends per episode). Same reliable
channel2 protobuf; send failures remain fatal. No deadline/decoder/audio/profile
changes, no FEC reconstruction yet. Request acceptance/server response still
requires hardware evidence; successful send alone is not acknowledgement.
Build succeeds; guard-8 backed up as build/debug/plus-vita-before-guard-9.vpk.
Uploaded same remote path with matching readback SHA256:
510FE07B2EE546B6DF82133E041E547122FFEF5349E19A8E7CA8F74FC8C86181.
NPSSO unchanged. Test remains60seconds, deliberate omission disabled. Awaiting
hardware validation, especially a natural-loss episode exercising retries.

guard-9 PASSED: build/debug/plus-vita-guard-9-result.log. Full60seconds,
3396decoded/3122displayed; seven natural loss episodes all resumed. Five episodes
exercised new corruption re-reporting, IDR resumes after580-609ms; remaining
two after99/109ms. Twelve incomplete frames,19stale packets, no pending recovery,
no native decoder error, finalcode0 and clean shutdown. This verifies tested
re-report path, not universal network-loss tolerance. Working VPK preserved as
build/debug/plus-vita-working-guard-9-baseline.vpk and remote backup
build/debug/plus-vita-before-audio-1-remote.vpk.

Next approved feature implemented: live-720p-refs5-audio-1. See
docs/AUDIO_PLAYBACK_TEST.md for bounded worker/Opus/PCM design and test criteria.
Native720p/recovery settings unchanged. Build succeeds; target codec selftest
will run on hardware before output. Uploaded same remote VPK, matching readback
SHA25673B586B84CDD8394AC43E2C01E81CCB51369F02DD2DA8E5BA8E580EE5B078BED.
NPSSO untouched. Awaiting first audible audio/video test; no controller input
or tuned A/V synchronization implemented yet.

audio-1 crashed during initialization before selftest-pass/port-start. Log/core
retrieved; audio-2-worker-init moves all codec initialization and test off Takion
default-stack callback to dedicated256KiB worker. Details/provenance/hash in
docs/AUDIO_PLAYBACK_TEST.md. Built/uploaded/readback verified, NPSSO unchanged;
native video and recovery untouched. Awaiting hardware verification, not yet
claimed fixed on device.

2026-10-10: Live 720p video, audio, full controller/touch mappings and clean
stop/reallocation have progressed through hardware tests. Input-7 completed
ten minutes with 45 successful IDR recoveries and no false timeout; input-8
removed automatic audio sample recording after the user accepted audio quality.
The next approved step is video FEC reconstruction using Pylux's existing
Cauchy backend, with all current reference guards retained. See
`docs/VIDEO_FEC_TEST.md` for implementation, host test results and the pending
hardware gate. Do not treat host reconstruction tests as a hardware fluidity
pass or relax the audited decoder/reference assumptions.

2026-10-10: First video FEC hardware run completed ten minutes cleanly, repaired
536 frames with zero repair failures, but still incurred 46 recovery episodes
and about 22 seconds total recovery wait. User approved the next targeted step:
three-frame assembly/reordering with a 20ms successor-triggered grace. Retain
late source/parity, release complete AUs strictly in order, and retain the
existing validated-IDR fallback for actual loss. Host ordering/timer/FEC tests
and VitaSDK packaging passed for `live-720p-refs5-video-reorder-1`; hardware
fluidity gate is pending. Details and metric limitations are in
`docs/VIDEO_FEC_TEST.md`. Display catch-up pacing is a separate subsequent step,
not bundled into this build; accepted audio, controls and recording-off stay.

2026-10-13: Reorder hardware gate passed for the tested 6m36s run: one 114ms
recovery, seven late-completed assemblies, no native/audio/FEC failure, clean
user stop. Network loss was also lighter; do not attribute all aggregate gains
to reordering. User approved the next display-pacing step. Implemented build
`live-720p-refs5-display-worker-1`: separate display worker, three screen buffers,
explicit pinned scanout ownership and protected recovery picture; no compressed
reference AU dropping, no decoder/reference/audio/receiver/credential changes.
Memory/UB/thread sanitizer tests and VitaSDK packaging passed, but the hardware
gate FAILED: the compressed preview queue filled after roughly 37 seconds.
The log does not prove the cause of the scheduling/arrival stall. User chose
to restore the tested reorder baseline, not continue this pacing experiment.
On 2026-10-13, restored `live-720p-refs5-video-reorder-1` source and the exact
preserved VPK; FTP readback matched the working hash. Accepted audio, controls,
FEC/reorder and decoder/reference guards are retained. Display remains the
original synchronous two-buffer implementation. See `docs/DISPLAY_PACING_TEST.md`
for the failure/rollback record. Further display experiments are deferred;
they require a new user decision and must not replace the accepted baseline.
