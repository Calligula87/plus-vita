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

## Required hardware checks — pending

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
