# Display pacing experiment: reverted

## Accepted baseline

The user accepted `live-720p-refs5-video-reorder-1` after the 2026-10-13
6m36s hardware run, with clean shutdown, no native/audio/FEC failure and
greatly improved streaming. See `VIDEO_FEC_TEST.md` for measured results and
their limitations. Keep the synchronous two-buffer display path, existing
decoder/reference guards, FEC/reorder, controls and accepted audio playback.

## Failed experiment, 2026-10-13

`live-720p-refs5-display-worker-1` separated display waiting from decoding,
using three screen buffers and explicit scanout ownership. Memory/UB/thread
sanitizer host checks and VitaSDK packaging passed. These did not establish
hardware stability.

The local private log `build/debug/plus-vita-display-worker-1-result-20261013.log`
identified the tested build. Video/audio started, then the run stopped after
roughly 37 seconds: `preview queue full; stopping instead of discarding reference
frames` and callback error 5 (overflow). The compressed queue reached 16;
the subsequent preview result was 16. This is local queue saturation, not proof
of malformed H.264, failed authorization or a cloud-side fault.

Decoder/display-worker errors and output-buffer starvation counters were zero.
Maximum display interval was 350352 us and audio-arrival gap 352721 us.
The log cannot distinguish an arrival burst from a main-loop scheduling stall;
it does not prove why the queue saturated. Do not silently discard compressed
reference frames or claim the experiment fixed on the basis of host tests.

## User-requested rollback, 2026-10-13

Restored the exact preserved reorder VPK to
`192.168.1.15:1337/ux0:/data/plus_vita_smoke.vpk`. Downloaded readback matched
SHA256 `6F4FEB9A3FAC9F90BC201AF0110DAA28979DC130E321F7CBE5F8BFA0D313E56B`
(1670238 bytes). The failed remote package was backed up locally as
`build/debug/plus-vita-before-stable-rollback-20261013.vpk`.
Credentials and other Vita data files were untouched.

Matching source restores the original synchronous presenter and catch-up
policy, removes worker/surface-pool wiring, and sets the working reorder build
tag. Accepted FEC/reorder, recovery timer fixes, rear L3/R3 and recording-off
remain. The experiment's complete modified sources/tests/plan are preserved
locally under `build/debug/display-worker-1-source-20261013/`, alongside its
VPK and private diagnostic log; they are not part of the stable source commit.

Rollback verification: VitaSDK rebuilt successfully. The rebuilt VPK's
`eboot.bin` is byte-identical to the preserved working package (SHA256
`AED525E955374BA5B4EB9DCCF199D1C3A427FAD2849A39F8E3B50243B43755A8`).
AddressSanitizer/UndefinedBehaviorSanitizer receiver tests passed: 200000
in-order/wrapped frames, late/expired/pressure cases, all 64 upstream FEC
vectors, 15 independently encoded loss cases and malformed/bounds rejection.
This verifies source restoration, not a new hardware run after reinstalling.

Further pacing changes are deferred at the user's request. They require a new
decision and a separately evaluated hardware test, with this baseline preserved.
