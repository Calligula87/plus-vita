# Visible video preview: congestion-feedback-1

## Measured transport feedback

Upstream Chiaki sends congestion reports every 200 ms. The custom probe
previously omitted them. This build adds authenticated reports at the same
interval, counting unique source and FEC units for every observed video
profile, including profiles not passed to the decoder. A frame's missing
units are counted when the next frame arrives; duplicate and stale packets
are ignored. It does not invent loss to force a lower resolution. It logs
report counts/send results and incoming CONNECTIONQUALITY target bitrate
and loss fields. Allocation remains 720p, default 1280x720 dimensions, 8000
kbps; valid NPSSO files remain retained. Decoder selection remains 360p.

This fixes a missing protocol mechanism, not a proven cause of the 720p-only
selection. The on-device test must show successful feedback sends and whether
profile index 2 appears. A repeat 720p-only result is not a playable-stream
success. Sources inspected on 2026-09-26:
- https://raw.githubusercontent.com/streetpea/chiaki-ng/main/lib/src/congestioncontrol.c
- https://raw.githubusercontent.com/streetpea/chiaki-ng/main/lib/src/frameprocessor.c

## 3000 kbps result and rollback

The one-variable 3000 kbps request reached allocation step 10 and received
HTTP 400. The Vita's installed `eboot.bin` hash matched the previous
`bitrate-360p-1` VPK, not the subsequently uploaded auth-retention build, so
the old app also consumed the auth file again. This build restores the
previously successful 8000 kbps allocation request while retaining the auth
file fix. Installing the VPK—not merely uploading it to `ux0:data`—is required
for the fix to take effect. The stream still observes profile indices and
does not claim 360p playback.

## One-variable bandwidth test

The `ux0:data/plus-vita-auth.json` credential file is now retained after a
successful parse, rather than consumed on every launch. Its read buffer is
still wiped in RAM. This avoids repeated NPSSO entry during device tests, but
the JSON credential remains on Vita storage until replaced or removed; keep
FTP access limited to the trusted local network. An invalid file is removed
so it can be replaced.

The `profile-observe-1` device run allocated at 720p/1280x720 and 8000 kbps,
initialized the Vita's nine-slot 360p decoder, then observed 6453 video
packets at server profile index 0 (720p), none at index 2 (360p), over 15
seconds. This build changes only the requested allocation bitrate to 3000
kbps. The 720p service setting, default 1280x720 client dimensions, and
profile-observation logic stay the same. If allocation fails, the log's HTTP
status establishes whether this bitrate request is rejected. If it succeeds,
the profile packet counts show whether the server actually selects 360p.
This is a selection experiment, not a claim that 360p is playable yet.

## 360p negotiation experiment

The profile audit showed H.264 profile 0 at 1280x720, profile 1 at
960x540, and profile 2 at 640x360. All three declare nine references and
nine buffered frames; the server actually sent profile index 0 in that run.
At 1280x720 this Vita accepts only five reference slots, and sustained
decoding fails with AVC out-of-memory.

The video-360p-1 allocation failed at step 10, before stream negotiation.
The video-360p-2 attempt also failed at step 10 with HTTP 400. This build
restores the known-good 720p service setting, its default 1280x720 client
dimensions and 8000 kbps bitrate. The allocation diagnostic records the HTTP
status code without logging a response body. The allocation-rollback-1 test
reached stream transport, but the first video packet used index 0 while the
Vita successfully initialized its 640x360, nine-slot decoder for index 2.
This build observes server-selected indices for up to 15 seconds. It discards
video packets for other indices without decoding them, counts all observed
indices, and can decode if the server switches to the 360p candidate. It does
not send a speculative profile-selection message. The client still
selects the best advertised H.264 profile no larger than 640x360, initializes
the decoder for that profile's full SPS buffering requirement, and accepts
video only if the server sends that selected profile index. If the server
rejects the request or sends another profile, the log says so explicitly;
the client never combines the wrong SPS with incoming video packets. The
service accepting and honoring 360p is still unverified on device.

## Profile audit

The revision 5 device log established that this 1280x720 H.264 stream
declares nine buffered frames. The native library accepts at most five
reference slots at that size; decoding then fails with AVC out-of-memory
after two displayed frames. STREAMINFO advertises three profiles, but the
preview had retained only profile zero. This build logs the resolution and
SPS metadata of every advertised profile and the index of the first video
packet actually sent. It does not force the server to select another profile.
The next physical test is to identify whether a lower profile is both
advertised and selectable; no slot-count change is claimed as a fix.

## Revision 5: measure the native slot ceiling

Revision 4 confirmed that this Vita rejects an eight-slot 1280x720 decoder
at library initialization (`0x80620802`), just as it rejected nine slots.
Four slots initialize but failed during sustained decoding in revision 2.
Revision 5 tries seven, then six, five and four slots, decreasing only when
native library initialization returns invalid-parameter. It logs the count
accepted by the actual device before any video is decoded. This is a bounded
capability probe, not proof that a stream declaring nine reference frames can
be decoded to completion. If all accepted counts still fail later with AVC
out-of-memory, the source profile must change; repeating slot guesses is not
a viable fix. Confirm the device says `video-preview-5` before testing.

## Revision 4: respect native decoder slot limit

The revision 3 physical log shows a profile 77, level 40, 1280x720 stream
declaring nine references and nine buffered pictures. Native
`sceVideodecInitLibrary` immediately rejected `numOfRefFrames=9` with
`0x80620802` (`SCE_VIDEODEC_ERROR_INVALID_PARAM`), before a video packet was
decoded. Revision 4 requests eight slots, the established Vita stream-client
setting, and logs the mismatch between the stream declaration and hardware
allocation. This is a bounded hardware experiment: it may decode successfully
or fail later if the stream actually uses all nine references. In the latter
case, requesting a different stream profile/resolution is the next path, not
another unsupported decoder allocation.

Verify `build=video-preview-4` on device and retrieve the diagnostic log.

## Revision 3: stream-sized H.264 reference pool

Revision 2 reached native hardware decode and displayed frames, but failed on
submission 13 with `SCE_AVCDEC_ERROR_OUT_OF_MEMORY` (`0x80620003`). Its FIFO
peaked at eight rather than overflowing, and normal decode calls took roughly
6--8 ms, isolating the failure to the decoder reference pool rather than the
transport or receive queue.

Revision 3 parses bounded metadata from the STREAMINFO H.264 SPS before the
native decoder is created. It uses the stream's declared reference and VUI
buffering requirements, with an established eight-slot Vita Chiaki floor and
a strict sixteen-slot ceiling. The same selected count is supplied to library
initialization, memory-size query and decoder creation. The diagnostic log now
records profile, level, coded dimensions, declared references, VUI buffering,
selected slots, queried bytes, submissions and native outputs.

Install and run as below, but verify `build=video-preview-3`. The expected
result remains ten seconds of moving, silent video followed by
`VISIBLE VIDEO PREVIEW PASSED`. A physical-device run is still required; the
successful local build does not establish that the larger native allocation
will succeed on this Vita.

## Revision 2: direct output and catch-up pacing

Revision 1 failed with 21 complete frames, zero incomplete frames, four
decoder submissions, two displayed frames and a full 16-frame queue. This
proves consumer backlog; it does not separately measure decoding and CPU
scaling cost. Revision 2 removes the per-pixel uncached CPU scaling path.

The AVC decoder is still initialized for the negotiated source resolution,
but its output surface is now 960x544, pitch 1024, directly in the non-displayed
CDRAM buffer. Output sizing follows the native approach used by
[Vita Moonlight](https://github.com/xyzz/vita-moonlight/blob/vita/src/video/vita.c).
This preview fills the screen, with a slight aspect-ratio adjustment for 16:9
source content. Native resizing and performance need physical validation.

All complete compressed frames remain in decode order. When at least two
compressed frames are waiting, the consumer can skip presenting an already
decoded image, reusing only the back buffer. Presentation still occurs at
least every 100 ms while decoding advances. Queue capacity remains 16; overflow
still reports failure. Audio playback and controls remain disabled.

The log includes native decode and input-preparation timings, display submit
and vblank wait totals/maxima, maximum queue wait and presentation-skip counts.
`cpu_scale_us=0` means that operation was removed; hardware resize time is
included in `decode_us`. Decoder deletion precedes output-buffer release.

Revision 2 used the `video-preview-2` build marker.

The details below describe the original revision 1 baseline.

This is a bounded physical-device test, not yet the playable-stream milestone.
It follows the confirmed `avc-diagnostics-2` test, which produced a 1280x720
hardware-decoded frame after two successful buffering submissions.

## Expected hardware result

Install `ux0:/data/plus_vita_smoke.vpk`, open Plus Vita, and run the same
cloud-allocation test on a PS4 title. After negotiation, the device should
display approximately ten seconds of actual decoded video, silently, then
restore the diagnostic screen. No controller input or audio playback is sent
by this preview. The final success text is `VISIBLE VIDEO PREVIEW PASSED`.

Retrieve `ux0:/data/plus-vita-allocation.log` after testing. Verify:

- Build marker is `video-preview-3`.
- `Stream probe preview displayed=1` appears, followed by a summary.
- Summary shows repeated displayed frames, received audio, `restore=0x00000000`
  and `result=0`.
- The user saw real video, and the diagnostic screen returned correctly.

## Implementation and limits

Complete compressed frames move through a mutex-protected, 16-entry FIFO.
The main thread decodes and scales them outside that mutex; the network
callback never waits for video decoding or display refresh. Frames are capped
at 1 MiB. Queue overflow stops the test explicitly, rather than silently
dropping reference frames. Late sequence numbers cannot reset newer assembly;
incomplete source frames are counted and discarded, never submitted partially.
FEC reconstruction and a multi-frame reorder window are not implemented yet.
The preview accepts profile zero only, and reports other profiles explicitly.

Native RGBA output is aspect-fitted to the 960x544 screen using two CDRAM
display buffers. Presentation waits for the buffer switch before reuse. The
previous diagnostic framebuffer is restored before freeing preview memory.
If restoration fails, buffers are retained to avoid freeing active scanout.

The first-frame deadline is 15 seconds. After first display, a three-second
video stall fails the test; success at ten seconds requires multiple displayed
frames, received audio and a displayed frame within the last second. These
checks do not establish smooth frame rate or acceptable visual quality.

## Local verification

The Vita Release VPK builds with warnings-as-errors for project code. GCC
`-fanalyzer` checks were run on the H.264 parser, decoder and stream probe
(upstream unused-parameter warnings suppressed for nanopb helpers). The H.264
parser also passes native ASan/UBSan tests covering slot selection, malformed
limits and every truncation of a valid synthetic SPS.
Physical display, sustained receive rate and cleanup still require the user
test above. No claims of on-device success should be made from compilation.
