# Gameplay control remapping with saved layouts and disabled inputs

Approved spec for [GitHub issue #37](https://github.com/Calligula87/plus-vita/issues/37). Published to GitHub on 2026-11-02.

## Problem Statement

Players cannot adapt Plus-Vita's fixed gameplay controls to their preferences or disable inputs that they trigger accidentally. Rear-touch assignments and the front-touchpad click are fixed, and Settings only explains the existing controls. Players also need a reliable way to save a layout without making menu navigation or the exit shortcut unusable.

## Solution

Add a control-mapping editor in Settings with one saved, app-wide gameplay layout. Players can assign each supported input to one game action or Disabled, independently enable or disable front-touch movement, and use Save, Cancel and Restore defaults. Changes take effect in the next gaming session. Menu controls and the physical two-second exit shortcut remain fixed.

The layout also supplies the configuration contract for DualShock 3 and DualShock 4 support on PS TV in #38.

## User Stories

1. As a player, I want to open a mapping editor from Settings, so that I can customize controls before playing.
2. As a player, I want to see each input and its assigned action, so that I can understand my layout.
3. As a Vita player, I want to remap physical gameplay buttons, so that I can choose comfortable assignments.
4. As a Vita player, I want to remap each rear-touch quadrant, so that I can choose how touch activates game controls.
5. As a Vita player, I want to remap the front two-finger touchpad-click input, so that it can perform my preferred game action.
6. As a player, I want an explicit Disabled binding, so that an unwanted input produces no game action.
7. As a Vita player, I want to disable front-touch movement independently of the click binding, so that I can control which touch information reaches the game.
8. As a player, I want each input to perform at most one action, so that my layout is predictable.
9. As a player, I want several inputs to share one action, so that I can use alternate ways to activate it.
10. As a player, I want a shared action to stay held until all contributing inputs are released, so that releasing one input does not cancel another.
11. As a player, I want to map inputs to face buttons, D-pad directions, shoulders, triggers, stick clicks, Options, Share and touchpad click, so that ordinary gameplay controls are accessible.
12. As a player using an analog trigger, I want its pressure preserved when assigned to a trigger action, so that proportional control remains available.
13. As a player, I want a Save action, so that I explicitly choose when a layout becomes my saved configuration.
14. As a player, I want to cancel edits, so that experimentation does not replace my saved layout.
15. As a player, I want Restore defaults, so that I can recover the standard assignments.
16. As a player, I want my saved layout retained after restarting the app, so that I do not need to configure it repeatedly.
17. As a player, I want a failed save to leave the previous layout intact and report the failure, so that I know which settings will be used.
18. As a player, I want each gaming session to use the saved layout from its start, so that its controls do not change unexpectedly.
19. As a player, I want menu navigation to remain fixed, so that a disabled gameplay button cannot lock me out of Settings.
20. As a player, I want the physical exit shortcut to work regardless of mappings, so that I can always leave a stream.
21. As a PS TV player, I want my supported controller inputs to use the same layout rules, so that customization works consistently across supported devices.
22. As a new player, I want the default Vita layout to match the accepted controls, so that upgrading does not silently change how I play.
23. As a player, I want the controller guide to explain remapping and its limits, so that I understand what is configurable.

## Implementation Decisions

- Extend the existing Settings/app flow with a mapping editor and draft-versus-saved configuration state. The editor is available before Play; editing during a live stream is outside this version.
- Maintain one persisted app-wide layout, not separate game or controller profiles. Use stable logical input/action identifiers so controller labels do not define storage semantics.
- Cover physical gameplay buttons, rear-touch quadrants and the front two-finger click input. PS TV integration adds supported external physical inputs through the same contract.
- Each input has exactly one destination or Disabled. Supported destinations are face buttons, individual D-pad directions, L1/R1, L2/R2, L3/R3, Options, Share and touchpad click. Stick axes retain the accepted behavior. The system PS button remains system-owned.
- Combine active contributors to the same digital destination. For trigger destinations, preserve analog pressure and combine contributors without prematurely releasing the destination; a fully pressed digital contributor produces full pressure. Analog-to-button conversion must have a documented, tested press/release rule.
- Front-touch movement is independently enabled or disabled. Suppressing movement suppresses game touch contacts and coordinates; the two-finger input remains governed by its own binding. Disabling that binding suppresses its mapped action.
- Save commits a valid complete layout. Cancel discards draft changes. Restore defaults resets the draft and requires Save to persist. Leaving without saving must not commit edits.
- Protect the last successfully saved layout from partial writes and failed saves. Report a save failure accurately. Missing configuration uses valid defaults; malformed or unsupported saved configuration must never create partial or unsafe mappings.
- Snapshot the saved layout when Play starts and keep it stable for that session. Loading, editing and persistence stay outside the input polling loop.
- Preserve the accepted Vita defaults. External controller defaults map their physical game controls to the matching game actions. Show controller-appropriate labels and explain inputs that are unavailable on the current hardware.
- Detect the exit chord from physical inputs before mapping: START + SELECT for two seconds on Vita and DualShock 3, OPTIONS + SHARE on DualShock 4. It cannot be disabled or reassigned. While the chord is held, suppress gameplay outputs derived from its two source inputs, preserving existing chord behavior.
- Keep the existing single input owner, feedback protocol, touch identity/release behavior and neutral-state shutdown ordering. This is an explicit addition of configurable mappings; it does not authorize changes to video, FEC/reorder, display, audio or stream-quality policy.
- Update the controller guide and feature descriptions to explain saved layouts, Disabled, defaults, session timing and the fixed exit shortcut.

## Testing Decisions

- Test observable behavior through the approved Settings-to-next-session contract: edit a layout, exercise Save or Cancel, start a session, feed input samples through the existing input-read boundary, and observe resulting game controller states. Prefer this integrated journey over private binding-table/helper assertions.
- Reuse the existing app workflow and account-persistence integration testing patterns for app effects, restart behavior and failed storage. Reuse the input-worker adapter and feedback tests for mapping, releases, touch identity and neutralization.
- Cover unchanged defaults; every supported source/destination category; Disabled; independent front movement/click settings; digital-to-trigger and analog-to-trigger behavior; and analog-to-button press/release boundaries.
- Verify many-to-one bindings with overlapping holds and releases, including mixed digital/analog trigger contributors. No destination may remain stuck or release while another contributor still holds it.
- Verify Save, Cancel, Restore defaults followed by Save or Cancel, restart persistence, missing/malformed configuration and save failure. A failed save must retain the previous valid saved layout.
- Verify that session-start snapshots stay stable, gameplay remapping does not change menu controls or stick axes, and the physical exit chord still stops streaming even when its individual buttons are remapped or Disabled.
- Exercise the real input-owner/feedback boundary for neutral releases and stop/restart; do not replace it with a second mapping-only implementation in tests.
- Require a real Vita comparison against the accepted stream build for defaults, custom mappings, simultaneous inputs, releases and stop/restart, with picture, audio and stability checked.
- Require the corresponding PS TV controller integration tests with DualShock 3 and DualShock 4 when combined with #38. Host tests or a successful VPK build alone do not establish an accepted stream build.

## Out of Scope

- Per-game layouts, multiple saved profiles or controller-specific profiles.
- Stick-axis remapping, sensitivity/deadzone tuning, macros, turbo, action sequences or multiple destinations per input.
- Editing a layout during a live stream.
- Remapping menu navigation, the physical exit chord or the system PS button.
- New controller drivers, pairing flows or PS2-controller adapters.
- Native DualShock 4 touchpad movement, motion controls, rumble or media/quality-policy changes.

## Further Notes

The maintainer confirmed the full scope and test boundaries on 2026-11-02 after two design rounds. This issue replaces the original feedback with the agreed implementation spec and remains open for implementation.

Coordinate the shared input contract with #38; #39 supplies the improved menu interaction used by the editor. These are related feature contracts, not permission to build independent mapping pipelines.

The accepted stream build remains the comparison and rollback reference. The existing architecture decision about preserving accepted streaming behavior still applies to input ownership, neutral/shutdown ordering and media policies; the fixed-mapping limitation is deliberately expanded by this feature.

Original feedback:

> Users requests the possibility to remap the control through the menu and apply this to the gaming session.
> A possibility is as well to disable completely one input leaving it blank.
