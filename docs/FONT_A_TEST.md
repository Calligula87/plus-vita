# Selected A renderer: normal app candidate

For #29, following the maintainer's 2026-10-23 physical comparison: “Both A
options on fonts are the best. Implement this option for the app.” This accepts
A for the setup and tile samples from `plus-vita-font-cover-compare-1.vpk`.
No numerical latency, framebuffer diagnosis or cover result was supplied.

A uses the Vita's installed PVF fonts through `font_vita.c`. The shared normal
app painter already used this path; rollout preserves its demonstrated pixels
and layout, names the common native typography roles, and delivers the normal
profile with the comparison menu/FreeType asset excluded. It does not substitute
a different typeface or claim a newly discovered raster fix.

| Role | Native pixels | Bounds/use |
| --- | ---: | --- |
| Micro | 14 | Build label (24px box), artwork placeholders (58px box) |
| Hint | 16 | Section label (26px), footer (28px), logo placeholder hint |
| Compact | 18 | Account summary and wrapped tile titles (48px) |
| Body | 20 | Instructions, progress, help, errors, details and confirmations |
| Control | 22 | Buttons (32–36px inner height), compact search/page controls (26px) |
| Brand | 24 | App name in a 38px box |
| Heading | 36 | Existing accepted heading in a 54px box |

All normal screens share this table/painter at 960×544 without framebuffer
scaling. Existing per-screen body widths/heights, wrapping and explicit ellipsis
remain. The system Latin/Japanese font lookup and bounded SDK bitmap fallback
remain; no Sony font files are extracted or packaged. The 256-entry/512KiB glyph
cache remains bounded and renderer ownership stays with the UI thread.

## What to install and test

Use only `plus-vita-font-a-1.vpk` from this handoff. Public log marker:
`build=ui-font-a-1`. The normal graphical Release profile has comparison OFF,
independent input ON and deferred logging ON. About has no A/B comparison menu.
The prior comparison and accepted stream VPKs remain available as rollback.

Delivered 2026-10-23 to `ux0:data/plus-vita-font-a-1.vpk`; FTP download/readback
matched SHA-256
`E454842F66CB79971DEA06E88CA5F2105FD0D46E0C35080A85A815E082E56395`
(1,182,125 bytes). Implementation: `b88f8bc`, diagnostic regression `54c2ce3`.
Full host suite passed; normal, historical diagnostic and comparison Vita
targets built. Independent Standards and Spec reviews found no actionable code
issues; the control-height documentation was corrected to include compact
catalogue controls. Package inventory and linked executable were checked:
normal build marker present, comparison menu/font absent, no account data,
logs, captures or artwork resources. The earlier accepted stream VPK checksum
was rechecked unchanged. Production device acceptance for #29 remains pending.

1. Check the actual welcome, About, setup instructions and import screens for
   complete small text, buttons and footer hints at normal viewing size.
2. Open the remembered account and catalogue. Inspect selected/unselected tiles,
   a long title, accents/non-ASCII text, details and missing-artwork placeholders.
3. Open search: accept a query, then cancel another keyboard entry. Check the
   retained query/selection and text/display restoration. Do the same with
   Account → Edit account label. The account summary retains its compact size.
4. Return through Account, confirmation/cancel and home. Check left-stick focus,
   held-stick release, Cross/Circle, touch paging and prompt feedback. If an
   error occurs, check its body and controls too; no real account files need be
   changed just to manufacture errors.
5. Report any remaining text/control regression by screen. Cover downloading
   remains in this build, but needs its own explicit hardware result for #30.

Native host journeys cover the real app/painter across welcome/help, setup,
progress/errors, catalogue/details, Account, confirmations and keyboard-result
accept/cancel. Repeated frames must be pixel-identical with bounded external
font work. Controlled external PVF glyph output checks preserve complete ink at
all seven sizes, including negative bearings, accents and Japanese; existing
long-title, fallback, cache-pressure, account, artwork and input tests remain.
Host PVF fixtures do not emulate the Vita's actual typeface. The maintainer's
comparison is the hardware selection evidence; production screen/keyboard
acceptance remains a separate device check.

This candidate does not enable graphical streaming or change the accepted
streaming policies. Logs, fixtures and package resources contain no personal
account text or credentials; do not share account JSON or the app-data folder.
