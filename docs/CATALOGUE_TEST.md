# Searchable catalogue candidate

This is development work for #16/#17, not a public release or an accepted stream
build. The accepted blue/gold design, font adapter and retained-JSON setup stay
in use. Hardware acceptance and the independent review are pending.

## Vita checks

1. Start with your existing JSON/saved sign-in. Confirm automatic authorization
   and the account heading. The Sony online ID appears only after authorization;
   an explicit local Account label overrides it. Do not include the name in a
   shared screenshot or log report.
2. Browse multiple pages using the left stick in both axes. Check initial focus,
   held-stick repeat and release. Cross and touch should open the chosen title's
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
disk. Downloads are capped at 2 MiB/10 seconds; PNG/JPEG decoding and the six-entry
thumbnail cache have fixed memory bounds. Failed artwork never disables details.

Host tests cover the real app workflow, render/font/input flow, optional artwork
metadata, actual PNG/JPEG decoder and asynchronous cache, plus the downloader's
HTTP/TLS/size/cancellation boundary with synthetic transport. These checks do
not establish Vita responsiveness, service artwork availability or streaming.
Keep the accepted font/response and streaming VPKs available for rollback.
