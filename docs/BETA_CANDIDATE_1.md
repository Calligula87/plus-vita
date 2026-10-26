# Anonymous beta candidate 1 — Vita handoff

The maintainer's later #35 graphics follow-up is implemented in
[candidate 2](BETA_CANDIDATE_2.md). This candidate 1 record and VPK are preserved.

Prepared for #22 with graphics spec #31 and implementation tasks #32–#34.
This is a test candidate. Publication remains blocked by #23; host tests and
privacy checks do not establish Vita acceptance.

## Exact artifact

The final VPK is 3,464,443 bytes; SHA-256:
`176AE0B3FB7C62B2773D0DB11290B6FFDB8D4C85901CC01BC15DD13FB036DBE8`.
The final identity and SHA-256 are recorded in
[`release/candidate-1.json`](../release/candidate-1.json). Its source commit is
`b111080975388a2f968d031ae4a80751781c20aa`. The public app version is
`0.1.0 beta candidate`, build `v0.1.0-beta-candidate-1`, with title ID
`PLVT00001` and accepted 720p independent-input/deferred-log/FreeType profile.

The local VPK is `local-artifacts/plus-vita-v0.1.0-beta-candidate-1.vpk`.
The private Vita staging location is
`ux0:data/plus-vita-v0.1.0-beta-candidate-1.vpk`; the staging record is local.
No VPK, private device screenshot, account JSON or diagnostic log is published.

Build and privacy commands are in [the release guide](../release/README.md).
The committed export excludes untracked inputs, disables checkout newline
conversion and records dependency/toolchain identities. The resource allowlist
uses exact committed public bytes, including nested dependency license pins.
The supplied originals remain local; only metadata-free derivatives are used.

## Software verification

- All 15 native host scripts passed, plus the deferred-logger input-worker run.
  These cover account persistence, real native UI flow/rendering, recovery,
  media bounds, input ownership and bounded/redacted deferred logging.
- All 34 desktop reference tests passed.
- The native branding journey checks the real image loader/full-screen painter,
  one-second boundary, Cross/touch dismissal, held-input suppression, one launch
  only, pending account effects and invalid-resource fallback. Real FreeType
  welcome/splash previews were visually inspected.
- The actual final VPK is checked for exact resource/app identity, icon format,
  image/archive metadata and raw/decompressed executable contents. Synthetic
  fixtures cover forbidden resources, credentials/canaries, private addresses
  and paths, UTF-16/compressed payloads, historical files, commit/tag messages,
  untracked-account exclusion and inherited Git newline conversion.
- Source, complete relevant root history, selected public documentation and the
  actual candidate must report no blocking findings. One exact historical
  diagnostic-helper workspace-path blob is documented in the reviewed policy;
  its current helper is script-relative. Credentials never receive exceptions.
  The sanitized [privacy report](../release/candidate-1-privacy.json) records
  the final combined pass with no blocking findings, including 107 commit
  messages, 1,240 historical blobs and one annotated-tag message.
- New hosted CI has not been run. Earlier CI output was inspected locally; its
  Linux SDK OpenSSL-link failure has a conditional link correction, whose Linux
  branch remains unverified. The candidate uses the tested local Windows SDK.

## Standards

The required independent review reported zero documented-standard violations
and zero actionable baseline smells, including follow-up exporter review. It
confirmed the accepted media/input policies and hardware/publication distinction.

## Spec

Review and clean-build checks found three preparation defects: history messages were
not scanned, the default Ninja path was resolved incorrectly, and checkout
newline conversion affected anonymous resource identity. All were corrected
and rechecked; zero findings remain. Final Vita acceptance remains pending.

## Vita test sequence

1. Install the staged VPK as an update in VitaShell. Confirm the blue/gold icon
   and the candidate identity in Settings/About. Keep the accepted comparison
   VPKs listed in [the stable baseline](STABLE_GRAPHICAL_BASELINE.md).
2. Cold-launch without touching controls: the complete splash should appear
   once and finish within one second. Repeat with Cross held, then with a
   screen tap. Dismissal must not activate the next screen. Back/navigation,
   stream return and Retry must not show the splash again.
3. Confirm the welcome logo replaces the placeholder and keeps readable text,
   focus, hints and controls. Check joystick, Cross/Circle and touch across
   screens; automatic saved sign-in and catalogue re-entry must still work.
4. Play, stop with the two-second START+SELECT chord, then relaunch a game.
   Recheck cancellation, Wi-Fi loss and sleep/wake recovery with explicit Retry.
   Confirm usable menus and normal picture, sound and controls against baseline.
5. Complete #23's 45-minute continuous play check and outstanding #16/#17
   account/catalogue edge cases. Follow the shipped reporting guide and copy
   logs privately before another Play/launch resets them. Report sanitized
   outcomes and the exact candidate identity; never paste account JSON/tokens.

Privacy and software preparation are separate from the maintainer's exact-artifact release
decision; no public release has been authorized.
