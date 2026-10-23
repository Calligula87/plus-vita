# Font comparison and Sony cover candidate

Work for #28 and #30. #29 remains blocked until the comparison has a recorded
Vita result and selected approach. Normal app screens still use the current
system-font renderer; FreeType is confined to the synthetic comparison screen.
The accepted blue/gold design, JSON instructions and streaming fallback remain.

## Current test package

Install only `plus-vita-font-cover-compare-1.vpk` for this test. The public log
marker is `build=ui-font-cover-compare-1`. This is a graphical Release comparison
profile with independent input and deferred logging enabled. A successful build
or host image does not establish Vita font or cover acceptance.

Candidate built from implementation commits `8af5bf8` and `e522e8b`:

- SHA-256: `78847FE1AC62C6675C0C32775FA30AC7A228A8963E7DED3BF299ED228B267CE5`
- Size: 1,878,691 bytes.
- Delivered to `ux0:data/plus-vita-font-cover-compare-1.vpk` on 2026-10-23;
  FTP readback matched the checksum. Earlier trial files are not this candidate.
- Full host regression suite passed, including actual pinned FreeType raster,
  native artwork integration, input/deferred logging and stream baseline checks.
  Default graphical, historical diagnostic and comparison Vita targets built.
- Independent Standards and Spec reviews completed. Both findings were fixed:
  malformed metadata classification and borrowed PVF context cleanup order.
  Regressions reproduced both failures before passing with the fixes.
- Package inventory contains executable/metadata, CA trust, licensed font and
  notices only; no account JSON, logs, screenshots or game artwork is bundled.

Hardware results are pending. #28 and #30 await this device test; #29 remains
blocked until a renderer is selected from the comparison.

## Font comparison

1. Open About Plus Vita, then Compare text rendering. This works without signing
   in and displays fixed synthetic content, never your account name or library.
2. A is the current system PVF renderer. Select Show B: FreeType to show the same
   view with FreeType 2.14.3 and the unmodified, openly licensed Noto Sans Regular
   font. Switch repeatedly and inspect body text, buttons, the 14/16/18-pixel
   samples, accents/descenders and footer hints at normal viewing distance.
3. Show tiles and compare A/B there too. Check the selected/unselected titles and
   deliberate long-title overflow. Noto Sans here covers Latin/Greek/Cyrillic;
   absent characters such as Japanese use the existing system fallback. Report
   that fallback separately from the FreeType Latin result.
4. Press Square once on each A/B setup/tile view. The app writes one of four fixed
   synthetic PPM screenshots in `ux0:data`: `plus-vita-font-A-setup.ppm`,
   `plus-vita-font-B-setup.ppm`, `plus-vita-font-A-tiles.ppm`,
   `plus-vita-font-B-tiles.ppm`. Repeating overwrites that view's file. These
   captures let us distinguish actual missing raster pixels from camera/LCD
   artifacts. Export is explicit storage work, excluded from menu timing.
5. Use the stick, Cross/Circle and touch. Confirm prompt focus/release and return
   to About/home. If B says FreeType unavailable, record that as a load failure,
   not a B font-quality result. No font choice changes the normal screens yet.

Report which renderer is clearer at each affected role, any chopped letters,
Japanese fallback, observed responsiveness, and which exact view was captured.
Only the synthetic comparison screen can export images. Ordinary Account and
catalogue screens cannot use this capture action.

## Real covers

1. Open your existing catalogue, browse several pages and open details. Confirm
   actual artwork appears where Sony supplies it and matches the selected title.
2. Confirm stick/touch, search/keyboard cancellation and details/back restoration
   remain responsive while images load. Missing/failed images must show readable
   placeholders and must not prevent navigation.
3. Recheck search or Account label keyboard return. This does not accept every
   persistence/error/Forget branch or final recovery test by implication.
4. Exit using the app action and keep the latest log before restarting. The log
   contains fixed artwork stage counts and separate A/B draw timing aggregates;
   it must not contain artwork URLs, game/account names, credentials or payloads.
   Never share JSON/account files or a whole app-data directory.

Covers download on demand from Sony servers. No game artwork is bundled or saved
on disk by the app. Existing request/decode/cache bounds and TLS verification
remain. Loading, genuinely missing metadata, rejected metadata and failed
downloads are distinct internally; user placeholders stay simple.

## Diagnosis and software evidence

An anonymous request to the existing supported Sony Apollo root and A–B category
returned normal product `images` arrays with types 10/12/13. Observed HTTPS hosts
include `vulcan.dl.playstation.net`, `apollo2.dl.playstation.net` and
`gs2-sec.ww.prod.dl.playstation.net`, all excluded by the preceding two-host
allowlist. No account credential was used. A representative image returned HTTP
200 and the real bounded decoder accepted its 1024-square PNG. This reproduces
the missing-URL failure mechanism; it does not prove every title has art or
replace the user's hardware check.

The native account integration regression first failed when the sanitized
response used an observed CDN host, then passed after the policy/resolver fix.
It now traverses the real account parser, downloader, decoder, cache and painter
through catalogue and detail pixels; only external HTTP/DNS/PVF calls are
simulated. Unknown/failed metadata and fixed failure categories remain covered.
Real Sony artwork/responses stay out of tracked fixtures and packages.

The FreeType regression uses the same pinned source as Vita. At every UI size,
the adapter's rendered alpha total equals an independent direct FreeType raster
of spaced ascenders/descenders/accents. It also covers warm reuse, cache pressure,
short text bounds, absent font files, A/B app navigation and screenshot output.
These tests do not reproduce Sony PVF output or establish the cause of its
device-only small-text appearance.

FreeType and font source pins, notices and coverage are documented with the
packaged font assets. Portions of this software are copyright (c) The FreeType
Project (https://freetype.org). All rights reserved.

Keep the previous catalogue VPK and accepted streaming VPK as rollback artifacts.
Graphical streaming is still #18. No long-play, release/privacy or final-beta
acceptance follows from this candidate.
