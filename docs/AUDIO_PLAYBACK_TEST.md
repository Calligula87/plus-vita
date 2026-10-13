# First audio playback gate

Preserves guard-9's 720p native decoder, loss recovery, profile and60-second
duration. Adds Opus1.6.1 fixed-point decoding on a separate128KiB-stack pthread;
no blocking audio output or decode occurs in the Takion receive callback.

Dependency downloaded from https://downloads.xiph.org/releases/opus/opus-1.6.1.tar.gz
(official download redirects to Xiph mirror), SHA256:
6ffcb593207be92584df15b32466ed64bbec99109f007c82205f0194572411a1.
Unmodified source in external/opus-1.6.1; COPYING also included in VPK.

Accept only advertised48kHz,16-bit,mono/stereo,480samples per Opus source unit.
Fail explicitly on unsupported header. Target codec silence roundtrip selftest
runs before opening audio port. Source units copied into16-frame bounded queue;
oldest queued unit discarded on overflow, with counter. No per-packet allocation.
FEC units are not played as ordinary audio. Sequence wrap/duplicates handled;
up to three missing units use Opus PLC, larger gaps reset decoder/staging.
Decoded480sample units accumulate into512sample (multiple64) Vita output blocks.
Volume50percent initially; physical Vita volume still applies. Stop joins worker
before drain/release/destroy. Overall result fails if worker failed/no blocks
played or drain/release failed. Native video guards unchanged.

Hardware test: install new VPK, choose a game with audible title/menu sound,
listen during60-second preview. Expect picture plus sound; controls not enabled.
Report whether sound was audible, crackling, delayed or drifting. Retrieve log:
audio header/selftest/start/first PCM and final summary, plus video/recovery
summary and finalcode. Passing output calls alone cannot establish audible sound
or A/V sync. This is a first playback gate, not a tuned jitter/sync implementation.

First hardware run CRASHED. Retrieved build/debug/plus-vita-audio-1-crash.log
and build/debug/audio-1-core.psp2dmp (remote1790525979). Last flushed event is
audio header channels2/rate48000/bits16/samples480. No codec-selftest-pass or
audio-start event, so initialization/selftest failed before live playback.
It runs on Takion callback's default-stack thread; the explicit128KiB worker
does not exist yet at this point. Stack pressure during encoder roundtrip is
a hypothesis, not confirmed by symbolicated core. Core is not plain ELF;
arm-vita-eabi-readelf cannot parse it. Preserve matching audio-1 ELF/VPK and
working guard-9 rollback. No fix/upload performed during crash diagnosis.

audio-2-worker-init moves decoder creation, encoder roundtrip selftest, and
audio port setup entirely onto the dedicated256KiB-stack worker. Start callback
only validates header, allocates bounded queue and launches worker; packets may
queue during initialization. Explicit logs precede decoder/selftest encode/decode
stages. Failure is propagated through queue push/final result. Stop joins before
freeing codec state; audio drain/release only if port was opened. Decoder/video
recovery unchanged. Stack-pressure hypothesis remains unconfirmed until test.
Build succeeds; crashed matching VPK/ELF saved in build/debug/plus-vita-audio-1-crashed.*.
Uploaded same remote VPK, matching readback SHA256:
58B66B9BBAD602864DF6DA8A4C3AD5CA59F07FEFA171D364901B6A12930BADE4.
NPSSO untouched. Awaiting60-second audible audio/video hardware result.

audio-2 completed successfully but user heard NO SOUND. Log retrieved as
build/debug/plus-vita-audio-2-result.log. Worker initialization/selftest pass,
5879 decoded units,5649 output blocks,147PLC,83stale,182queue drops, peak16,
failed0; drain/release0, finalcode0. Video3486decoded/3226displayed, three loss
recoveries. This confirms calls succeeded, not audible nonzero PCM or routing.
Next diagnostic gate should measure decoded PCM peak/RMS/nonzero samples and
use a short low-volume local reference tone to separate remote silence/packet
selection from Vita output routing/mute. Queue overflow also needs investigation
but cannot alone explain total silence. No new build/upload for this review.

audio-3-tone-levels diagnostic gate:256ms400Hz triangle reference tone,1024
peak(-30dBFS before existing50percent port gain),512sample faded edges. Uses
same port/format as remote audio.24tone blocks recorded separately, excluded
from remote decoded/output counters and PCM measurements. Tone runs on worker
before dequeuing stream; can add bounded startup queue drops, not a playback
performance benchmark. Track each channel's cumulative peak/RMS/nonzero/sample
counts for actual decoded source units (exclude selftest, tone and PLC). Log
first/every1000 units and final. No raw audio or secrets persisted. Video and
stream decoder unchanged. Build and arithmetic bounds check passed; uploaded
same remote package with matching readback SHA256:
DF751D8FFD567100BF2EF821994B2A3F05BEE5FB3B0695B1B56D58E19FFF8EB8.
Previous package preserved as build/debug/plus-vita-audio-2-silent-baseline.vpk.
NPSSO unchanged. User should report reference beep audible AND game audio
audible separately; hardware result pending.

audio-3 hardware run passed technically: build/debug/plus-vita-audio-3-result.log
from Vita FTP192.168.1.16:1337, finalcode0. Tone output24blocks completed;
user has not yet reported whether it was audible. Decoded remote PCM almost
entirely silent: cumulative peak L34/R28 of32768 and integer RMS rounds0 by
end; nonzero counts stopped increasing after first1000 decoded units, through
5330 decoded. Stream output API succeeded5647blocks. Video3560decoded/3267
displayed, one loss recovery. This strongly suggests remote source silence or
wrong audio source, not simply an AudioOut API failure. Tone audibility is still
essential to distinguish Vita speaker route/mute. Audio queue dropped701 and
PLC694 (some due initial tone), a separate latency/jitter issue; do not equate
technical pass with audible working audio.

User reported no clearly audible reference tone. audio-3's256ms tone had
1024/32768 peak before50percent port gain, too brief/quiet to rule out routing.
audio-4-audible-tone now plays two bounded triangle bursts at400Hz and800Hz,
separated by silence, total640ms; peak8192 before unchanged50percent port gain,
one-block fades. Remote PCM measurements and decoder/video path unchanged.
Build succeeded. Previous VPK saved as
build/debug/plus-vita-audio-3-tone-baseline.vpk and remote backup as
build/debug/plus-vita-before-audio-4-remote.vpk. Uploaded/readback verified
SHA256 FCC1592E6F3E0974E24CC7A89C5C834D486C0A5ACB1BEDEC2768B5CC2EB81028.
NPSSO untouched. Still needs user report of audible tones vs game audio.

audio-4 hardware run passed: build/debug/plus-vita-audio-4-result.log, final
code0; user clearly heard both generated tones. Tone60blocks completed through
the same Vita output port, confirming speaker/headphone output path works.
Remote PCM:5932 decoded units; peak L241/R209 of32768, final RMS L5/R5.
Nonzero sample counts reached50378/49348 by decoded unit1000 and did not grow
through end, indicating mostly zero-valued source audio after the early segment.
Video3599decoded/3286displayed, no loss episodes. Audio output5649blocks,
100queue drops (tone startup may contribute), no API failure. Next investigate
whether source/game actually emits audio and whether the selected incoming
audio units/codec contain it; do not alter Vita output route solely on this log.

User clarified that the stream showed a game waiting for X to start. This may
explain silence and means the source-audio diagnosis is not yet conclusive.
audio-5-cross-test adds only Vita CROSS down/up feedback during the bounded
preview, using Pylux's existing feedback sender/history packets. It logs both
edges. Check whether CROSS advances the game, then compare remote PCM peaks
and audibility during actual gameplay. No video profile or decoder changes.

audio-5-cross-test hardware run PASSED: build/debug/plus-vita-audio-5-cross-result.log.
User pressed CROSS, advanced past the waiting screen, and heard game audio.
The log has four CROSS down/up pairs. PCM stayed near-silent before input, then
rose to peak L3738/R4540 and final nonzero samples L1215127/R1214838. Video
3232 displayed, two recovered packet-loss episodes, final result0; audio
5507 decoded /5682 output blocks, no worker failure. This establishes that
the prior silent run reflected a silent source screen, not a broken Vita audio
path. There were 575 audio queue drops and 555 concealments; assess audible
continuity in a later test rather than calling this fully tuned A/V playback.

input-1-basic extends the proven feedback sender to Vita face buttons, D-pad,
both sticks, L/R shoulders and Start/Select as PS4 Options/Share. L2/R2, stick
clicks, touchpad and PS button remain unmapped. Each button-mask change and the
total number of controller updates are logged. Video/audio settings unchanged.

User confirmed all basic gameplay controls work. input-2-rear-triggers maps
rear touch left half to digital L2 and right half to digital R2 (255 while at
least one finger touches that half; zero after the last finger lifts). A finger
on each half holds both. The split uses the rear panel's reported active-area
midpoint, not a hardcoded screen width. Existing touch sampling state is
restored at preview exit. If rear initialization fails, basic physical controls
remain available and the log reports rear_L2_R2=0. Video/audio unchanged.

input-3-ten-minutes extends the live preview from 60 seconds to 10 minutes so
L2/R2 can be exercised during actual gameplay. The normal live path no longer
records five-second H.264 samples or writes rejected-AU video files; capture-
only diagnostic code remains dormant with PROBE_CAPTURE_ONLY=0. The 8 MiB
capture buffer is not allocated in live mode. No decoder/audio/control mapping
changes. Built successfully; previous remote VPK backed up at
build/debug/plus-vita-before-input-3-remote.vpk. Uploaded and read back the
same ux0:data/plus_vita_smoke.vpk, SHA256
B8845093D0207C62C28C9D8E6EB79E12C1A765D292B9580D92234F22353F57FB.
On request, removed the 16 exact plus-vita-capture/rejected .h264 files from
ux0:data and verified zero .h264 files remain there. Other data, credentials,
logs, and VPKs were untouched. No Vita-side backup of deleted captures made.

User confirmed basic gameplay/rear controls work and requested PS4 touchpad.
input-4-front-touchpad maps Vita front touch contacts to PS4 touch coordinates
(0..1920 by 0..942) using queried front-panel active bounds. Up to two
contacts retain stable touch IDs through movement and lift. Two simultaneous
contacts also hold the PS4 touchpad-click button; one contact is touch/swipe
only. Rear touch remains L2/R2. Feedback history now emits both release and
new press if a touch slot is replaced within one update. Build succeeded;
hardware touchpad behavior still needs verification. No video/audio/duration
or recording change from input-3.
Previous remote input-3 VPK was backed up locally at
build/debug/plus-vita-before-input-4-remote.vpk (SHA256 B8845093D0207C62C28C9D8E6EB79E12C1A765D292B9580D92234F22353F57FB).
The new VPK SHA256 is 25335D8C9686EFEB3144F964DEC87CA86004DFA3754C03CFFBA74A97A8C50E10.
FTP at 192.168.1.16:1337 initially timed out. After the user restarted it,
the new VPK was uploaded to ux0:data/plus_vita_smoke.vpk and read back;
SHA256 matched the local build. Installation and front-touchpad behavior still
need hardware verification.

## Audio continuity comparison (audio-6)

After the user confirmed gameplay, controls, and manual stream reconnect,
they reported distorted/fragmented sound. The 122-second diagnostic copied to
`build/debug/plus-vita-audio-quality.log` reports 12,161 received source units,
11,976 decoded units, 127 PLC units, 38 stale units, 147 queue drops, and a
full 16-unit queue. Decoded PCM peaks around 12,425/32,768 with no output API
failure, so clipping or failed Vita audio output is not the leading explanation.
The 640 ms diagnostic reference tone ran before draining a queue that could
hold only 160 ms, creating avoidable startup overflow; it also kept the worker
behind live audio. This is a likely contributor, not proof of the sole cause.

`live-720p-refs5-audio-6-no-tone-queue32` removes the normal-stream reference
tone, increases the bounded queue to 32 units (320 ms maximum), and adds queue,
drop, concealment, and stale counters to each periodic PCM-level log. The
10-minute limit, cloud allocation, video decoder, controls, volume, Opus
format, and loss-concealment policy are unchanged. VitaSDK build succeeded.
The working prior VPK is backed up as
`build/debug/plus-vita-before-audio-6-remote.vpk`, SHA256
C9F3D8D0BDD69D45BE8A51281CC6D1146BEB692C4A31EF01DE21695CE6FD9FCD.
The new VPK was uploaded to `ux0:data/plus_vita_smoke.vpk` on
192.168.1.15:1337 and downloaded back with matching SHA256
A20EE50E7F982B0CFF6812346E9E3067A96E1E59102D2A86DA63F3194B34A145.
The credential file and all other Vita data were left untouched. Hardware
installation and a like-for-like game-audio comparison remain to be done.

## Audio continuity timing comparison (audio-7)

The audio-6 hardware run (`build/debug/plus-vita-audio-6-hardware-20261009.log`)
lasted about 94 seconds. It confirmed queue=32 and no reference tone; the final
audio counters were 9271 decoded, 27 concealed, 51 stale, zero queue drops,
queue peak 24, and zero audio-output errors. Twelve video recovery episodes
also occurred. Since the user still heard fragmentation, the startup tone and
queue overflow were not the whole cause.

`live-720p-refs5-audio-7-5mbps-timing` keeps 720p, 48 kHz stereo Opus, queue
size, controls, and playback behavior unchanged. It requests 5000 rather than
8000 kbps from cloud allocation and logs audio arrival gaps, empty-queue wait
durations, output-call timing, queue age, and sequence-gap resets. Arrival gaps
are delivery timing, not by themselves proof of lost packets. No audio payloads
or credentials are recorded. Build output is
`build/debug/plus-vita-audio-7-5mbps-timing.vpk`, SHA256
`919E3D9AF1231CF240A28C2ED0142385EE77472216D42794613AB56A97245F4F`.
On 2026-10-10 the installed audio-6 VPK was backed up locally as
`build/debug/plus-vita-before-audio-7-remote-20261010.vpk` (SHA256
`A20EE50E7F982B0CFF6812346E9E3067A96E1E59102D2A86DA63F3194B34A145`).
The audio-7 VPK replaced only `ux0:data/plus_vita_smoke.vpk` at
192.168.1.15:1337; FTP readback SHA256 matched the local build. Credentials
and all other Vita files were untouched.

For the hardware comparison, use the same game and network for at least a
minute after gameplay audio begins, note whether fragmentation improves, then
end the stream with START+SELECT. Retrieve `ux0:data/plus-vita-allocation.log`
and compare the timing counters with the user's audible result. Do not treat
this diagnostic build as an audio fix unless the hardware comparison supports
it.

## Audio-7 regression and audio-8 correction

The audio-7 test log (`build/debug/plus-vita-audio-7-failed-20261010.log`)
reached live transport, but a 5000 kbps allocation caused the server to send
video profile index 1 (960x540). The client selects profile index 0 (1280x720),
so no video frame was submitted or displayed and the preview deadline expired.
The server reported target_bitrate=5000000. This was a test-design regression,
not evidence that the audio timing instrumentation caused the failure.

`live-720p-refs5-audio-8-8mbps-timing` restores the previous 8000 kbps request
while retaining only the audio timing counters. The build is
`build/debug/plus-vita-audio-8-8mbps-timing.vpk`, SHA256
`B13AD534D3986F30C1A5261C682C0257E6197BD1C16A4CA15B35FDAA90CB2BD8`.
For a useful audio diagnosis, install this corrected build and stream the same
game until active gameplay audio is heard. No sound-quality fix is claimed yet.
The failed audio-7 VPK was backed up from the Vita as
`build/debug/plus-vita-before-audio-8-remote-20261010.vpk`. Audio-8 replaced
only `ux0:data/plus_vita_smoke.vpk` at 192.168.1.15:1337; FTP readback SHA256
matched the audio-8 build. The credential file was not changed.

## Audio-8 hardware result and audio-9 queue-drop correction

The audio-8 hardware log is `build/debug/plus-vita-audio-8-test-20261010.log`.
It streamed for about 165 seconds and ended cleanly. The audio worker decoded
16265 packets without a codec or output-port failure, but the queue gradually
grew from 18 to 32 packets. Once full, it locally evicted 201 old packets and
then concealed 195 apparent sequence gaps. Most of those gaps track the local
evictions, not demonstrated network loss. Concealing a locally evicted 10 ms
packet spends another 10 ms at the audio output and can keep the queue full,
causing more evictions. The user also heard fragmentation through headphones.

`live-720p-refs5-audio-9-local-drop-fix` marks locally evicted packets in the
queue. The worker skips concealment for those intentional evictions while
retaining Opus concealment for packets actually missing on arrival. A new
`skipped_local` timing counter distinguishes these cases. The 720p/8000 kbps
video request, volume, Opus format, controls, queue capacity, and audio output
port settings are unchanged. This is a targeted correction for the measured
queue feedback loop, not yet a confirmed cure for all audible fragmentation.

VitaSDK build succeeded. The package is
`build/debug/plus-vita-audio-9-local-drop-fix.vpk`, SHA256
`418649DF4CE281BACEF5102ED53443BC9EBCDC90C28CB6B6BE25F8B90966096B`.
The prior audio-8 VPK was backed up as
`build/debug/plus-vita-before-audio-9-remote-20261010.vpk` (SHA256
`B13AD534D3986F30C1A5261C682C0257E6197BD1C16A4CA15B35FDAA90CB2BD8`).
Audio-9 replaced only `ux0:data/plus_vita_smoke.vpk` at 192.168.1.15:1337;
FTP readback SHA256 matched the build. The credential file was not changed.

Retest in active gameplay long enough to pass the prior approximately two-minute
queue-overflow point, ideally with headphones. Note when any fragmentation
occurs. After stopping the stream cleanly, retrieve
`ux0:data/plus-vita-allocation.log`; compare `dropped`, `concealed`,
`skipped_local`, `queue_now`, and output-gap counts. The joystick menu test is
inconclusive; test the sticks separately in an active race.

## Audio-9 result and delayed 10-second diagnostic capture

The audio-9 hardware log (`build/debug/plus-vita-audio-9-test-20261010.log`)
ended after about 215 seconds with 21463 decoded units, 7 concealed, 16 local
queue drops, 15 of those skipped rather than concealed, and no codec or audio
port failures. This largely removed the previous drop/conceal feedback loop.
The user confirmed that the sticks work in Assassin's Creed Origins, but still
heard a high-frequency or mosquito-like artifact in the audio. The timing log
cannot identify its spectral content.

`live-720p-refs5-audio-10-delayed-capture` therefore buffers a bounded audio
sample without changing playback. The window starts no earlier than 120 seconds
after the first live audio packet, captures up to 10 seconds of decoded stereo
48 kHz 16-bit PCM and the corresponding processed Opus packets, and writes two
files under `ux0:data` only after clean stream shutdown. The filenames are
`plus-vita-audio-<timestamp>.wav` and `plus-vita-audio-<timestamp>.opuspack`.
The latter has an 8-byte `PVOPUS1\n` magic, little-endian sample rate (u32),
channel count (u16), 480-sample frame size (u16), record count (u32), then
records of sequence (u16), packet length (u16), and packet bytes. This captures
only the short diagnostic window, not credentials or the whole game session.
If the stream ends before two minutes, no audio sample is written. A partial
sample may be written if it ends during the 10-second window. The capture
buffers are bounded at about 2.2 MB; allocation failure disables capture but
does not stop playback.

VitaSDK build succeeded. Local VPK:
`build/debug/plus-vita-audio-10-delayed-capture.vpk` (SHA256
`75DC619FF7ECCD318F326344B2C5E334FBEADB35DD96E6A03E63DF08C8F955DE`).
The previous audio-9 VPK was backed up as
`build/debug/plus-vita-before-audio-10-remote-20261010.vpk` (SHA256
`418649DF4CE281BACEF5102ED53443BC9EBCDC90C28CB6B6BE25F8B90966096B`).
Audio-10 replaced only `ux0:data/plus_vita_smoke.vpk` on 192.168.1.15:1337;
FTP readback hash matched. Credentials and controls were untouched. For the
hardware test, stream until sound is audible, continue beyond two minutes for
at least ten more seconds, then end cleanly with START+SELECT. Retrieve the
WAV, opuspack, and allocation log via FTP for spectral and packet comparison.

## Audio-10 capture result and audio-11 Vita output test

The audio-10 hardware log is
`build/debug/plus-vita-audio-10-test-20261010.log`. The delayed window
completed 480000 decoded PCM frames and 1000 consecutive Opus packets. The
captured 48 kHz stereo WAV is
`build/debug/plus-vita-audio-1791631842.wav`; its corresponding packet file is
`build/debug/plus-vita-audio-1791631842.opuspack`. The log records 14590
decoded packets, one concealed packet, eight local drops, and no decode or
audio-output failures over roughly 145 seconds. The WAV has no clipped samples.
The user played that WAV on a phone and heard clean sound, despite hearing
high-frequency distortion from the Vita during the same test. This points to
the Vita playback path after decoded PCM, although it does not prove the exact
cause.

`live-720p-refs5-audio-11-vita-output` changes only Vita playback settings:
the main audio port now uses 960 frames per output block (20 ms) rather than
512 frames, and output-port gain is unity rather than 50%. The output timing
log uses a 30 ms stall threshold appropriate to the new block duration and
prints the port configuration. Opus decoding, network transport, video,
controls, and the delayed capture are unchanged. The 960-frame block follows
the Vita Moonlight audio implementation and satisfies VitaSDK's multiple-of-64
frame requirement. This is a hardware A/B test, not a confirmed fix.

VitaSDK build succeeded. Local VPK:
`build/debug/plus-vita-audio-11-vita-output.vpk` (SHA256
`B28B67E56708DCDDB15FCC093863FE8282F0F35652365015165897479B12283C`).
The previously installed audio-10 VPK was backed up as
`build/debug/plus-vita-before-audio-11-remote-20261010.vpk` (SHA256
`75DC619FF7ECCD318F326344B2C5E334FBEADB35DD96E6A03E63DF08C8F955DE`).
Audio-11 replaced only `ux0:data/plus_vita_smoke.vpk` on 192.168.1.15:1337;
FTP readback SHA256 matched the audio-11 build. Credentials were not changed.

For the hardware test, start with the Vita's physical volume low because the
output gain changed from 50% to unity. Stream the same game with headphones
long enough to hear sustained gameplay audio. Compare the mosquito-like or
fragmented sound with audio-10, then end cleanly with START+SELECT. Retrieve
`ux0:data/plus-vita-allocation.log` and compare the output stall, queue,
decode, and audioout counters. If sound becomes worse, the saved audio-10 VPK
is the rollback package.

## Audio-11 hardware result

The user confirmed that the audio-11 build sounds correct on the Vita. The
retrieved log is `build/debug/plus-vita-audio-11-test-20261010.log`. It confirms
the port opened at 48 kHz stereo with a 960-frame output block and unity gain.
The stream ran for about 135 seconds and ended cleanly through user stop. Final
audio counters were 13379 decoded units, 6694 output calls, nine concealed
units, five local queue drops (all five skipped rather than concealed), and
zero audio-output failures. Maximum audio-output call time was 18449 us, below
the 20 ms block duration; the log also reports 17 output-gap stalls above its
30 ms threshold. This successful run validates the new playback settings on
the tested Vita and game. The two output-setting changes have not been isolated
from each other, so it does not establish which one cured the audible artifact.

## Audio recordings retired

After the successful input-7 ten-minute test on 2026-10-10, the user requested that automatic audio samples stop. Build `live-720p-refs5-input-8-no-audio-capture` removes the recording buffers, sample copies, and WAV/Opus packet file writes. Playback settings and diagnostic counters stay in the normal allocation log. Six generated WAV files and their six companion `.opuspack` files were backed up to `build/debug/audio-captures-backup-20261010/` and removed from `ux0:data`.
