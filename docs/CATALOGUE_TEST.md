# Searchable catalogue candidate

This is development work for #16/#17, not a public release or an accepted stream
build. The accepted blue/gold design, font adapter and retained-JSON setup stay
in use. Hardware acceptance is pending; independent software reviews completed.
The current-session diagnostic marker is `build=ui-catalogue-1`.

Candidate: `plus-vita-catalogue-1.vpk`, graphical Release, independent input and
deferred logging enabled. SHA256:
`4F3DFA1951388338BAC5EECA2EC291343662ECBF52E31723B6F75A7CC10731CC`.
Source commits: `21a680d`, `19125f8`, `8e52ba5`.
The authorized device FTP transfer timed out; delivery is not yet verified.

## Vita checks

1. Start with your existing JSON/saved sign-in. Confirm automatic authorization
   and the account heading. The Sony online ID appears only after authorization;
   an explicit local Account label overrides it. Do not include the name in a
   shared screenshot or log report.
2. Browse multiple pages using the left stick in both axes. Check initial focus,
   held-stick repeat and release. Touch Prev/Next to reach later pages without
   the stick. Cross and touch should open the chosen title's
   details. D-pad presses must not move menu focus.
3. Use Triangle or Search. Search a partial English title in another letter case,
   cancel the keyboard, clear the query, and try a query with no results. Try a
   supported non-ASCII title. The keyboard accepts at most 32 characters; ASCII
   case folding does not promise Unicode case folding.
4. Open details from a filtered page, then Circle back. Confirm the same filter
   and selected title. Go home/About/Account and return to the catalogue too.
5. Check complete text on tiles, headings and details. Long titles must wrap or
   deliberately elide. Confirm readable placeholders for absent/failed covers
   and responsive navigation while covers load or Wi-Fi is unavailable.
6. Details show available platform information and a clear Play action. A title
   missing required launch metadata must explain why Play is unavailable. The
   actual graphical streaming connection belongs to #18 and is not enabled by
   this catalogue-only candidate.

The artwork worker permits only public PlayStation HTTPS CDN hosts, verifies
TLS, sends no account credentials, rejects redirects, and retains no artwork on
disk. HTTP transfers are capped at 2 MiB/10 seconds; native DNS uses a one-second
timeout with one retry and checks cancellation immediately afterwards.
PNG/JPEG decoding and the six-entry
thumbnail cache have fixed memory bounds. Failed artwork never disables details.

Host tests cover the real app workflow, render/font/input flow, optional artwork
metadata, actual PNG/JPEG decoder and asynchronous cache, plus the downloader's
HTTP/TLS/size/cancellation boundary with synthetic transport. These checks do
not establish Vita responsiveness, service artwork availability or streaming.
Keep the accepted font/response and streaming VPKs available for rollback.

The DNS adapter uses the native [VitaSDK resolver API](https://github.com/vitasdk/vita-headers/blob/master/include/psp2/net/net.h)
to avoid the unbounded libc resolver and libcurl's threaded resolver on Vita.

## Software validation

All native host regression scripts and the combined input/deferred-log variant
passed under the existing sanitizer harnesses. Both graphical and historical
diagnostic Vita targets built. The updated diagnostic identity/privacy test
passed. The package contains only its executable, SFO, public CA bundle and four
dependency licenses; no account files or game artwork are bundled. This is a
candidate inventory check, not the full publication/privacy gate in #22.

## Standards review

The reviewer identified artwork DNS running outside the HTTP timeout. The fix
adds bounded native resolution and tests the real resolver/downloader with
simulated platform calls. Re-review found no new actionable API, ownership,
memory or privacy defect. Repeated numeric limits remain a possible
maintainability improvement, not a documented rule breach.

## Spec review

The reviewer identified missing touch access to later catalogue pages. Prev/Next
and a fourth-title regression resolve it; re-review found no further actionable
spec mismatch. Account-name behavior matches #16's amended requirements.

Review summary: one actionable finding per axis, both resolved. Vita acceptance
remains open. #18 is natively blocked by #17; streaming is not enabled in this
candidate. No 45-minute play, recovery or release-readiness claim is made.
