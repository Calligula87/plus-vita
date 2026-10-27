# LiveArea format trial 1 — accepted on Vita

On 2026-10-27 the maintainer reported the requested device test passed: “All
good!” This accepts the LiveArea display check for the exact artifact below and
closes #35. The combination of template formatting and page reload was tested;
the report does not independently isolate the cause or establish unreported
long-session/account/catalogue/recovery checks under #23. The accepted format
and VPK are preserved as the comparison for the v000 follow-up.

The maintainer's candidate 2 photo still shows the system LiveArea background.
Read-only inspection found the installed and decrypted metadata copies of all
three LiveArea resources identical to candidate 2. Stale on-disk artwork is
ruled out. Neither package tests nor matching installed resources establish the
actual SceShell display result.

The candidate 2 XML uses LF line endings. The
[VitaSDK XML notes](https://github.com/vitasdk/samples#notes-on-xml) specify UTF-8
and CRLF. This controlled compatibility trial uses expanded CRLF serialization
with the same parsed layout, attributes, references, images and version. It
changes only `sce_sys/livearea/contents/template.xml` inside the VPK; all other
members, including `eboot.bin`, are byte-for-byte unchanged.

Artifact: `plus-vita-v0.1.0-beta-candidate-2-livearea-format-1.vpk`, 4,021,415 bytes.
SHA-256: `E643DD43B64FB887C47B9851A9331339668D6077779E5F13F6ADCB9C3B6D1A98`.
Runtime source remains `44ccbef3d834066ef7392df2429c6b29aa593e61` and build identity
`v0.1.0-beta-candidate-2`; template source is
`055f1f7a2d063fe64a58d1cdab4e439c353534a1`. The
[variant identity](../release/livearea-format-1.json) distinguishes this resource
revision from the preserved original candidate 2.

The package format check fails on the original and passes on this variant.
The controlled repack reproduces exactly, enforces semantic equality before
output creation, and binds the base VPK to candidate 2's recorded checksum.
Anonymous export preserves the committed XML bytes through a `-text` Git
attribute. Package/privacy checks and synthetic leak-rejection fixtures pass.
Native stream/UI tests were not rerun: no executable or runtime resource pixels
changed.

## Standards

Independent review: zero documented-standard violations and zero actionable
smells. The trial preserves earlier artifacts and binds repackaging to the
recorded candidate 2 checksum.

## Spec

Independent review: zero concrete specification defects in the controlled trial.
Actual LiveArea appearance is unresolved. The package tests and unchanged parsed
layout do not establish that formatting caused the native failure.

## Reproduction and device check

Reproduce with a new output path:

```sh
python tools/repackage-livearea-template.py \
  local-artifacts/plus-vita-v0.1.0-beta-candidate-2.vpk \
  build/livearea-format/plus-vita-v0.1.0-beta-candidate-2-livearea-format-1.vpk
python tools/test-branding.py \
  build/livearea-format/plus-vita-v0.1.0-beta-candidate-2-livearea-format-1.vpk
```

The private FTP staging file is
`ux0:data/plus-vita-v0.1.0-beta-candidate-2-livearea-format-1.vpk`; download
readback matches. Direct metadata mutation was rejected by automatic approval
review as outside the earlier inspection authorization. No installed/protected
files were changed; this VPK is for ordinary VitaShell installation.

Install it as an update. Close the Plus Vita LiveArea page using the top-right
peel gesture, then open its bubble again and inspect the background. Report that
actual display result. Serialization is a hypothesis, not a confirmed root
cause. The maintainer has now confirmed the branded page. #23's release validation
and publication gate remain pending. No cache dumps, device photos, account data
or raw logs are published.
