# Stable graphical Vita test baseline

Accepted by the maintainer on 2026-10-24 after repeated Vita play tests. This is
the stable **working baseline** for the next development tasks, not a public
v0.1.0 beta release or completion of the final release gate.

| Field | Tested value |
| --- | --- |
| VPK | `plus-vita-play-18-interruption-1.vpk` |
| Vita location | `ux0:data/plus-vita-play-18-interruption-1.vpk` |
| Local copy | `local-artifacts/plus-vita-play-18-interruption-1.vpk` (ignored by Git) |
| Size | 2,417,984 bytes |
| SHA-256 | `2240D73D887BCE2AD1E5277510D1722289E5D50480794D1688DBFAC98229F51A` |
| Source commit | `fa4e406421c2e4d04361198b4f4ec481854e49a0` |
| Local Git tag | `vita-graphical-stable-2026-10-24` (not published) |
| Stream profile | Accepted 720p independent input and deferred logging profile |

The graphical Play flow, retained sign-in JSON, font, covers, controls and
interruption screen are the accepted working experience. The graphical build
has no application-imposed ten-minute play cutoff. One retrieved hardware log
records a 857,539 ms (14m17s) play session ending by the user's START+SELECT
request with a clean disconnect. The maintainer then retested the interruption
screen candidate and reported no interruptions and good play and shutdown.
The cause of an earlier exit is unknown because the next Play replaced its log.

Keep this exact VPK and the older accepted stream artifact available for
comparison. Do not publish either as the final beta yet. Remaining release work
is tracked in #16 (account edge cases), #17 (catalogue edge cases), #19
(connection/Wi-Fi/sleep recovery), #20 (bounded privacy-safe diagnostics),
#22 (anonymous package and publication gate) and #23 (final 45-minute Vita
validation). #24, direct Sony sign-in research, is a separate later task.

The user's sign-in JSON and diagnostic logs stay private and are never part of
this baseline artifact or Git history. Copy the log before another Play or app
launch when investigating an interruption.
