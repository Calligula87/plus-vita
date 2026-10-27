# Candidate 2: LiveArea and menu background follow-up

## LiveArea follow-up: accepted on Vita

On 2026-10-27 the maintainer confirmed the requested format-trial device test
passed. #35 is closed; see [the exact accepted format variant](LIVEAREA_FORMAT_TRIAL.md).
The prior investigation below is retained as history. The v000 follow-up
supersedes the visible build naming while preserving the accepted resources.

The maintainer's subsequent Vita photo still shows the system multicolor
LiveArea background and default gate. #35 is reopened for this portion; the
earlier package pass and FTP readback did not establish display correctness.

A read-only device inspection confirms that all three files installed under
this app's `sce_sys/livearea/contents` match candidate 2 exactly. The separate
`ux0:appmeta/PLVT00001` copy is PFS-protected: ordinary FTP returns encrypted,
allocation-padded data. These bytes cannot establish whether the cached template
is stale or invalid. After the maintainer opened the metadata folder decrypted,
the cached XML, background and startup image were also verified byte-for-byte
against candidate 2. An old on-disk resource copy is therefore ruled out; the
display failure remains unresolved.

The template does not follow the VitaSDK sample's documented CRLF convention.
A template-only format variant is now prepared in expanded CRLF XML, with the
same parsed layout, references, artwork and executable. The package format test
fails on candidate 2 and passes on the variant. This is a compatibility trial,
not proof that serialization caused the native display failure. Source:
[VitaSDK XML notes](https://github.com/vitasdk/samples#notes-on-xml).

Direct mutation of installed/protected metadata was rejected by automatic
approval review as outside the earlier inspection-only authorization. No device
files were changed. The alternative is a separately named VPK installed normally
through VitaShell; the issue stays open for the next actual display result.

---

Implements the maintainer's #35 request under #23. The earlier candidate's
general positive visual feedback is recorded without inferring specific recovery
or 45-minute acceptance. The supplied device photos remain private.

`plus-vita-v0.1.0-beta-candidate-2.vpk`, 4,021,412 bytes, runtime source
`44ccbef3d834066ef7392df2429c6b29aa593e61`, SHA-256
`EEA909AE81B219BBB2B1E3563CD0F1A3C13CC74EA131A334C4202E1E8118A984`.
Build identity is `v0.1.0-beta-candidate-2`; title identity and the accepted
720p input-worker/deferred-log/FreeType profile are preserved. See the pinned
[identity](../release/candidate-2.json) and [privacy report](../release/candidate-2-privacy.json).

The original branded wide artwork is now installed as the LiveArea background
with a matching Start/Continue image. All in-app menu screens use a cached,
full-screen blue background with the logo, lettering and their reflections
removed. The old welcome logo and placeholder are absent. A prepared dark
reading surface preserves text/focus contrast without runtime blending.
The installed app icon and opening splash remain unchanged.

The clean plate was edited with built-in imagegen using only the supplied public
artwork. Its [source and exact prompt](../vita-client/assets/branding/README.md)
are retained with the project; no device photo or account screen was an input.

All 15 native host scripts and the deferred-logger input-worker run passed.
The actual FreeType welcome preview was visually inspected. Tests verify cached
background rendering on multiple screens, obsolete-logo suppression, focus,
navigation, splash behavior and invalid-image fallback. The final VPK passes
the exact resource/executable/source/history privacy gate, synthetic rejection
fixtures and native LiveArea XML/indexed image format/size/metadata checks.
All five runtime artwork derivatives reproduce byte-for-byte after the shared
fitting-helper refactor. No new hosted CI run is claimed.

## Standards

Independent review: zero documented-standard violations. One minor duplication
suggestion was resolved with the shared image-fitting helper; follow-up review
confirmed no remaining actionable smells.

## Spec

Independent review: zero concrete specification defects. LiveArea appearance
and exact-candidate behavior on hardware remain for #23.

Install `ux0:data/plus-vita-v0.1.0-beta-candidate-2.vpk` as an update. Check the
menu backdrop/readability and absence of the old logo; press PS to inspect the
branded LiveArea, then Continue and verify responsive navigation. Recheck cold
launch splash/skip, remembered sign-in and Play/stop/re-entry. #23's longer
session and remaining account/catalogue/recovery gates remain pending.

The privately staged FTP copy was downloaded again; its SHA-256 matches the
local candidate above. The local transfer record is retained outside Git.

Candidate 1 and all stable/fallback VPKs are preserved. No public binary release
is authorized. Account JSON, tokens, logs, private photos and local compiler
output are not attached to GitHub.
