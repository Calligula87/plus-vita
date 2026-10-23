# Corrected font selection: B

The maintainer clarified that the preferred second comparison display was
**B: FreeType / Noto Sans**. The comparison button said “Show A” while B was
displayed, which led to the opposite interpretation. The A normal build showed
clipped tops in “DEVELOPMENT BUILD,” “YOUR ACCOUNT IS CONNECTED” and “Open.”

The normal app now creates the same B font context used on the comparison
screen. It uses the pinned FreeType 2.14.3 and licensed Noto Sans Regular for
available glyphs, with the system PVF path for unsupported characters. The
960×544 layout and native role sizes stay the same. The comparison profile
still starts on A and can switch to B. The historical diagnostic interface is
unchanged. The normal VPK bundles only the documented open font and notices;
it includes no account JSON, logs, captures or game artwork.

Test `plus-vita-font-b-1.vpk` only. The public log marker is `build=ui-font-b-1`.
The 2026-10-23 candidate is 1,877,535 bytes, SHA-256
`1A71596D572E0C5611D59887A6D5252D7EC14F6E0286362847EA7BD6E3BDB8D9`.
The full host suite and native Release build passed. Package inspection found
the licensed B font and notices, excluded comparison UI, and found no account
files, logs, captures or artwork resources.
Confirm “DEVELOPMENT BUILD,” “YOUR ACCOUNT IS CONNECTED” and the O in “Open
catalogue” have complete tops on the physical Vita. Also check setup, Account,
catalogue tiles, details, footer hints, search and label keyboard return,
responsive controls and Japanese/fallback titles. If a particular role remains
clipped, name the screen and text. Sony covers were separately accepted under
#30; this test checks that they still appear while navigating.

The original A VPK, comparison VPK and accepted stream VPK remain rollback
artifacts. Host raster comparisons and simulated PVF checks do not replace the
physical normal-screen result. Graphical streaming remains a separate task.
