# Anonymous beta candidate gate

Issue #22 prepares a candidate; it does not authorize public publication.
Graphics spec #31 and implementation slices #32–#34 are part of this candidate.
The accepted graphical/recovery and streaming VPKs remain unchanged outside Git.

The release profile identifies itself as **0.1.0 beta candidate**, requires the
accepted 720p independent-input/deferred-log and FreeType profile, and rejects
diagnostic/comparison interfaces, test URLs and local certificates. Experimental
replay-file loading is excluded from release startup. The normal live decoder,
media, input, font, account and recovery policies remain in place.

## Build from anonymous committed inputs

Commit the intended implementation first. Export only that commit and the pinned
dependencies into a new directory under `build/`; private untracked files and
working-directory configuration are not copied. The build removes inherited
compiler flags, specifies every release switch explicitly, maps compiler source
paths to public prefixes and normalizes VPK timestamps/attributes. A source and
dependency identity manifest accompanies the artifact.

```sh
python tools/build_anonymous_candidate.py --output build/anonymous-beta-22 \
  --sdk /path/to/vitasdk --cmake /path/to/cmake --ninja /path/to/ninja
```

Use the pinned VitaSDK and dependency revisions recorded with the candidate.
The command retains compiler diagnostics locally and prints only sanitized
status/artifact identity. It performs no network acquisition or publication.

## Verify the actual artifact

```sh
python tools/publication_gate.py --vpk build/anonymous-beta-22/plus-vita-v0.1.0-beta-candidate-1.vpk \
  --report build/anonymous-beta-22/privacy-report.json \
  --materials README.md docs/ACCOUNT_SETUP.md docs/REPORTING_ISSUES.md release/README.md
python tools/test-branding.py build/anonymous-beta-22/plus-vita-v0.1.0-beta-candidate-1.vpk
python tools/test-publication-gate.py build/anonymous-beta-22/plus-vita-v0.1.0-beta-candidate-1.vpk
```

The first gate examines tracked source, complete relevant Git history, selected
public materials and the actual unpacked VPK. It checks an exact resource
allowlist, resource identities, app identity, metadata and raw/decompressed SELF
segments. Reports contain fixed finding categories/counts, never matched values.
An unreadable/unsupported input fails closed. Synthetic fixtures demonstrate
that forbidden files, metadata, credentials, addresses, paths and raw/compressed
or UTF-16 executable canaries are rejected. A clean HEAD cannot hide a historical
canary. The export fixture proves untracked account-like input is excluded.

Automated scanning complements the explicit build-input/resource review; it is
not proof that arbitrary unknown secrets can be recognized. Real account-access
exposure blocks publication and requires sanitized remediation/revocation as
appropriate. Never reproduce a credential or rewrite public history without
authorization.

The reviewed-history policy records one historical diagnostic helper containing a
developer workspace path. The current helper uses its script-relative directory.
Only that exact historical path finding is acknowledged; credentials and
canaries never receive an exception. No history was rewritten.

Run the complete native host suite and 34 desktop reference tests, inspect the
real-font branding previews and relevant CI output, and retain sanitized evidence
and the final checksum. No raw device logs, private screenshots, user account
JSON or working directories are release/publication inputs.

## Human release gate

Every report keeps publication unauthorized until #23's exact-candidate Vita
validation, including the 45-minute run and remaining #16/#17 edge cases.
Verify installed icon, brief/skippable splash and welcome logo on Vita as part
of that gate. Host rendering, a package build and a software privacy pass cannot
establish hardware acceptance. No automatic upload/release job is provided.

SELF layout validation follows the primary
[VitaSDK header](https://github.com/vitasdk/vita-toolchain/blob/master/src/self.h)
and [writer](https://github.com/vitasdk/vita-toolchain/blob/master/src/vita-make-fself/vita-make-fself.c).
