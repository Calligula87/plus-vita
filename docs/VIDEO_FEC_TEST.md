# Video packet repair test

Current build: `live-720p-refs5-video-reorder-1`.
First FEC build: `live-720p-refs5-video-fec-1`.

## Baseline and objective

The input-7 log (`build/debug/plus-vita-input-7-result-20261010.log`) completed ten minutes cleanly but had 96 incomplete frames, 45 IDR recovery episodes and 1182 discarded dependent frames. It observed 37764 parity packets with reconstruction off. The objective is to repair missing source units before reference continuity breaks, reducing IDR recovery freezes. The separate display catch-up policy still applies.

## Implementation

The receive assembly stores distinct source and parity units. When their combined count reaches the expected source count, `video_fec.c` calls the existing Pylux `chiaki_fec_decode` with its GF-Complete/Jerasure Cauchy backend. Source units retain their two-byte, big-endian padding prefix; shortened units are zero-padded to the shared FEC width. Reconstructed prefixes restore actual packet lengths. All geometry, restored lengths, original source bytes and implicit zero padding are checked before newly allocated source slots commit. Partial reconstruction is never submitted.

GF(256) repair supports at most 256 total slots, with scratch storage capped at 2 MiB. Unsupported/malformed repair is logged and falls back to original source reception or the existing validated-IDR recovery; allocation failure ends the stream safely. Normal complete-source assembly stays unchanged. The H.264 syntax/reference guard and audited five-slot configuration remain required for repaired frames.

The backend is initialized by a fixed, independently calculated two-erasure self-test before live transport. Repairs log wire frame, restored unit count and elapsed microseconds; the final summary logs attempts, repaired frames/units, failures, total and maximum repair time. Congestion feedback still counts actual received packets, so repairs do not conceal wire loss from the server.

The first FEC increment retained one assembly frame: parity arriving after the receiver had moved to a newer frame could not repair the old frame. Its follow-up adds the bounded reorder window described below. Display pacing remains unchanged. Audio recording remains removed, and input mappings/playback parameters are retained.

## Completed checks

`sh tools/test-video-fec.sh` ran in the existing Ubuntu WSL environment. AddressSanitizer covered the backend and wrapper; UndefinedBehaviorSanitizer also covered the wrapper and tests. Passed:

- All 64 independently supplied upstream FEC reference vectors, both raw backend decoding and actual video packet length restoration.
- Fifteen loss cases using an independent scalar parity encoder, including missing first/last source units, missing parity and variable lengths/unaligned widths.
- Excessive loss, inconsistent widths, invalid restored headers/padding, unsupported geometry and oversized scratch requirements: no partial output or changed input slots on rejection.

VitaSDK compilation and VPK packaging succeeded. This does not yet establish repair speed or improved fluidity on hardware.

First FEC VPK uploaded to `192.168.1.15:1337/ux0:/data/plus_vita_smoke.vpk` and downloaded back; SHA256 matched `69E5A1C18CCC8BCE48E18E521D3BB97E331D7DAB659C404ABD672212245E9876`. The previous remote package was backed up as `build/debug/plus-vita-before-video-fec-1-remote-20261010.vpk` and matched the capture-free input-8 baseline.

## First FEC hardware result, 2026-10-10

`build/debug/plus-vita-video-fec-1-result-20261010.log` completed ten minutes with clean shutdown, no native decode/audio failure, and all 46 IDR recoveries resumed. Repaired 536 frames / 554 source units with zero repair failures; mean repair time 836 us, maximum 14632 us. There were 62 incomplete frames, 1303 discarded dependent frames and 22075 ms of recovery waits (maximum 1160 ms). Displayed 30421 frames; 3835 decoded display updates were skipped by the unchanged catch-up policy.

This proves functional FEC on hardware, not satisfactory fluidity. The remaining recovery wait is similar to the preceding run; network conditions differ, so lower incomplete/skipped counts alone do not establish a causal improvement. The single-frame receiver cannot use late packets; the old log does not establish how many recoveries that causes.

## Approved follow-up: short late-packet window

`video_reorder.c` manages exactly three frame assembly slots. In-order complete frames release immediately, without a fixed playout delay. An unfinished or wholly absent predecessor receives a 20000 us grace starting with the first observed successor. Further packets do not extend that deadline. A late source/parity packet can complete the retained predecessor, including via the existing FEC wrapper, before any successor enters the H.264 guard/decode queue. Ready successors cannot overtake it.

Expiry retires the incomplete assembly or missing range, without advancing H.264 reference state. The next complete AU still detects the gap and requires validated-IDR recovery. Capacity pressure retires only enough old state to free a slot; it never evicts a ready AU or silently discards a reference frame. The ordinary output/IDR deadlines remain. Clock sampling and the same probe mutex serialize callback and main-loop timer drains; no decode, scaling or vblank waiting runs in the callback. Expiry occurs on the next callback/main-loop poll, so scheduling can make measured waits exceed 20 ms; telemetry records actual elapsed time rather than claiming a hard real-time maximum.

Packet storage per assembly is capped at 2 MiB including parity/prefixes, plus the existing 1 MiB final AU cap and 2 MiB FEC scratch cap. Congestion reception keeps a separate three-generation ledger with the same grace, counts distinct actual source/parity receipts, and never counts reconstructed source packets as received. Unknown frame geometry does not manufacture packet-loss counts. The most recent ledger frame is finalized once a successor permits retirement, as in the previous report policy.

The final reorder summary records accepted late packets, late-completed assemblies (`saved_frames`, not a claim that each avoided an IDR), expired partial/missing frames, pressure drops, peak occupancy, blocked wait total/max and complete-AU ready-to-release hold total/max. `retired_packets` includes ordinary parity arriving after an already released AU; it is not all harmful packet loss. Ready-hold timing excludes network transit, decoding and display waits; it is not end-to-end input latency.

### Follow-up host validation

`sh tools/test-video-reorder.sh` passed under AddressSanitizer and UndefinedBehaviorSanitizer: 200000 immediate in-order frames with repeated 16-bit wrap; duplicate/stale filtering; late completion before the successor; exact wait/hold timestamps; expiry without more packets; whole missing-range retirement; fixed deadlines; full-window pressure; empty/partial-frame and backward-clock handling.

`sh tools/test-video-fec.sh` also passed all 64 reference vectors and malformed/bounds checks. The 15 independent parity cases now additionally verify repaired predecessor release before a completed successor across wrap. VitaSDK compilation/VPK packaging passed. Hardware improvement remains pending; do not change audio, controls, decoder reference guards or display pacing in this test.

### Follow-up deployment

Uploaded the reorder build on 2026-10-10 to `192.168.1.15:1337/ux0:/data/plus_vita_smoke.vpk`, replacing the sole old Plus Vita package in that directory. Readback matched SHA256 `6F4FEB9A3FAC9F90BC201AF0110DAA28979DC130E321F7CBE5F8BFA0D313E56B` (1670238 bytes). The old remote FEC build was downloaded to `build/debug/plus-vita-before-video-reorder-1-remote-20261010.vpk`; its hash matched the first FEC package. Local new build: `build/debug/plus-vita-video-reorder-1.vpk`. No credentials, captures, audio settings or input mappings were changed during this deployment. Hardware test pending.

## Hardware test

1. Install `ux0:data/plus_vita_smoke.vpk` and start the same game/network as the baseline.
2. Play for eight to ten minutes. Check video freezes, controls including rear L3/R3, and audio. Hold START + SELECT for two seconds to stop before the ten-minute preview cap, or let the cap finish normally.
3. Enable FTP and retrieve `ux0:data/plus-vita-allocation.log`.
4. Confirm the FEC self-test passed, `reconstruction=on`, successful repaired frames, no decoder/reference guard error, and clean final shutdown. Compare incomplete frames and recovery episodes per minute with the baseline, accounting for changing network loss. Inspect maximum repair time and queue peaks for added processing stalls.
5. For the reorder build, verify build tag `live-720p-refs5-video-reorder-1` and the reorder summary: peak at most three, accepted late packets/saved frames, actual wait/ready-hold maximum, and expired/pressure counts. Compare recovery pauses as well as controls responsiveness. Display catch-up skips can still remain because their policy has not changed.

The baseline capture-free VPK remains at `build/debug/plus-vita-input-8-no-audio-capture.vpk` for rollback.

## Reorder hardware result, 2026-10-13

`build/debug/plus-vita-video-reorder-1-result-20261013.log` identifies the correct reorder build and completed 395669 ms before user-requested stop, with result=0, clean disconnect and no decoder/audio failure. The user reported greatly improved streaming. Observed: 21376 displayed / 23408 decoded, 2032 replaced display updates (8.68%), one incomplete AU, one IDR recovery (114 ms, eight dependent AUs discarded), and 56 successful FEC repairs / zero failures. Seven assemblies completed with accepted late packets (18 late packets total). Window peak=2, pressure drops=0; blocked wait total=102201 us, maximum=20218 us. Ready-to-release hold maximum was 69 us. Decode mean about 6.94 ms, compressed-queue wait maximum 90726 us.

Functional late-packet recovery is verified, not merely inferred from the better aggregate run. Network loss was lighter than the previous ten-minute run, so the entire improvement cannot be attributed to the window. This is the accepted working baseline. The subsequent isolated display-worker hardware test failed and was reverted on 2026-10-13 at the user's request; see `docs/DISPLAY_PACING_TEST.md`. The working reorder VPK was restored over FTP with the original hash verified. Receiver window/FEC, audited decoder/reference assumptions, accepted audio, controls and original synchronous display are retained.
