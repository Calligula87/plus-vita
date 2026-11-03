# Feedback controls validation — #37, #38, #39

Review baseline: `3884180b7a8c458022b0d2e257f77260c0286250`.
Development build identity: `feedback-controls-37-38-39-1`.

The existing accepted v000 package and its recorded identity remain the
comparison/rollback reference. A successful build or host test does not make
this candidate an accepted stream build.

## Software boundaries

The tests exercise the agreed app/UI-flow and controller-read-to-feedback seams:

- App workflow: D-pad and stick focus, repeat timing and fixed menu controls.
- UI flow/painter: physical-style touch down/move/up/cancel sequences, visible
  press state, slide-away cancellation and the timed transition input barrier.
- Settings-to-next-session: real layout persistence, Save/Cancel/defaults,
  failed replacement, malformed/missing records and mapped gameplay output.
- Input reader/owner: handheld defaults and touch identity, shared bindings and
  releases, independent front movement/click, analog thresholds and pressure,
  DualShock 3/4 extended controls, selected port, disconnect neutralization and
  reconnect gating. The real feedback sender proves that disconnection releases
  an active stick without ending the input owner, then permits new input after
  a neutral reconnection.
- Real FreeType previews: mapping overview, source/action selection and both
  controller guides. Generated images stay under the ignored build directory.

Run the native host scripts under Linux/WSL, including
`tools/test-app-controls.sh`, and the input-worker variant with
`PLUS_VITA_INPUT_TEST_DEFERRED_LOG=1`. Compile the graphical Vita target and
retain the candidate package checksum with the final software results.

## Software verification

Verified source commit: `66cc53dac6e4bcbac5f3fc7a7e749871c6ce5587`.

- All 16 native host scripts pass with their normal address/undefined-behavior
  sanitizer settings; the deferred-logging input-worker variant also passes.
- All 34 desktop reference tests pass on Node 24.21.0.
- The graphical VitaSDK development build compiles and links with the accepted
  input-worker/deferred-log and FreeType configuration. Diagnostic and font
  comparison interfaces are disabled; this is not a release-profile package.
- Real-font mapping overview, source/action pages and DS3/DS4 guides were
  rendered and inspected. The mapping overview help text fits without clipping.
- Review findings for idle-menu sleep, controller handoff and join ownership are
  resolved. Both duplication concerns are resolved. Final shutdown fault handling
  received follow-up lifecycle review and external-boundary fault tests.

Initial candidate: 4,038,545 bytes, SHA-256
`E569C4C57C779693F452B8FEC678C9DEF6A9BA8577607AC3882001C92E86B294`.
Host logs, compiler output and generated previews remain local in the ignored
build directory. These software results do not establish hardware acceptance.

## Required hardware checks — pending

Ownership faults are fail-safe: stop retries a failed join once. If joining
fails twice, it terminates the process before sender/context teardown. The host
fixture intercepts this system termination boundary to verify that stop cannot
return live-owned resources to its caller.

Record the exact VPK hash, firmware, device/controller type and connection method
for each run. Do not infer PS TV support from system pairing or synthetic samples.

| Device | Required comparison |
| --- | --- |
| Vita | v000 defaults; D-pad/stick menus; touch feel/cancellation; transitions; search/details/back; saved custom layout and Disabled; front/rear contacts and releases; fixed exit; stop/relaunch; unchanged picture/audio/stability |
| PS TV + DualShock 3 | System pairing; selected port; all face/D-pad/stick inputs; independent L1/L2/R1/R2/L3/R3; actual trigger values; remapping and touchpad-click assignment; START+SELECT exit |
| PS TV + DualShock 4 | Same full matrix, using OPTIONS/SHARE labels and exit; verify actual pressure and extended-button exposure |

For both PS TV controllers, disconnect with buttons/triggers/sticks held. Confirm
all game input releases while the stream stays alive. Reconnect while holding
controls; input must remain neutral until all controls return to neutral. Then
verify a fresh press/movement, repeat the cycle, and check a second controller
does not take over while the active one remains connected. Test stop/restart and
normal network/sleep recovery separately.

Keep the same active controller through menus, connection and gameplay, including
when a second controller appears at a lower-numbered port. Check touch-down in an
idle menu, sleep/wake, then finger release: the old target must not activate.

Keep these issues open for the hardware acceptance results. No upload, release,
device installation or acceptance of this candidate is implied by software work.

## Mapping-label clarification

The maintainer reports that the menus seem OK and clarified that the mapping
confusion came from physical PS TV button rows appearing before the Vita rear
touch zones. This is scoped feedback, not full hardware acceptance.

Physical L2/R2/L3/R3 inputs now say "button" and show "PS TV only" on Vita.
Rear inputs explicitly say "Rear touch" and identify their corner. The input
list also states the Vita default layout: top L2/R2, bottom L3/R3. All inputs
remain editable; the stored defaults and gameplay mapping policy are unchanged.

The Settings-to-session test, real-font preview checks and Vita build pass.
The updated candidate is `build/vita-feedback/plus_vita_smoke.vpk`, 4,038,842
bytes, SHA-256
`331BF5D44841203F97673C683B81191FB15206073CF820D12BFBEE3BF75B37E4`.
