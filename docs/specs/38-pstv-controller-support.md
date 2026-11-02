# PS TV gameplay support for DualShock 3 and DualShock 4

Approved spec for [GitHub issue #38](https://github.com/Calligula87/plus-vita/issues/38). Published to GitHub on 2026-11-02.

## Problem Statement

PS TV players need to use their standard controller to navigate Plus-Vita and play streamed games. The app currently uses handheld-oriented controller sampling and obtains L2/R2/L3/R3 from Vita rear touch, so system pairing alone does not establish correct gameplay support on PS TV.

The original report mentioned a PS2 controller. The maintainer clarified that the intended scope is the base PS TV controller, DualShock 3, plus DualShock 4; PS2-controller adapters are not part of this feature.

## Solution

Support one active, system-paired DualShock 3 or DualShock 4 on PS TV for app navigation and ordinary gameplay controls, including physical L1/R1, L2/R2 and L3/R3. Use the shared layout configuration from #37, preserve the fixed physical exit shortcut, and explain supported controls and limitations in the controller guide.

When the active controller disconnects, release all game inputs, keep the cloud stream running and allow reconnection. Require neutral controls before gameplay input resumes.

## User Stories

1. As a PS TV player, I want to use a system-paired DualShock 3, so that I can play with the base supported controller.
2. As a PS TV player, I want to use a system-paired DualShock 4, so that I can play with that supported controller too.
3. As a PS TV player, I want pairing instructions to point to system settings, so that I know where to connect my controller.
4. As a PS TV player, I want the D-pad and left stick to navigate app menus, so that I can select games without touch.
5. As a PS TV player, I want Cross to select and Circle to go back, so that app navigation is consistent with Vita.
6. As a PS TV player, I want face buttons and D-pad directions to reach the game correctly, so that ordinary gameplay works.
7. As a PS TV player, I want both sticks to work independently, so that movement and camera controls remain available.
8. As a PS TV player, I want physical L1/R1 and L2/R2 to be distinct, so that shoulder and trigger actions are not confused.
9. As a PS TV player, I want physical L3/R3 to work, so that I can use stick-click actions without a rear touchpad.
10. As a player using an analog trigger, I want exposed trigger pressure preserved, so that games can use proportional input.
11. As a DualShock 3 player, I want START and SELECT mapped consistently, so that I can use Options and Share game actions.
12. As a DualShock 4 player, I want OPTIONS and SHARE mapped consistently, so that the labels on my controller match the guide.
13. As a PS TV player, I want supported physical inputs available in the shared remapping editor, so that I can customize or disable them.
14. As a PS TV player, I want to assign an ordinary button to cloud touchpad click, so that a game requiring that click remains accessible on either controller.
15. As a PS TV player, I want a fixed two-second physical exit shortcut, so that gameplay remapping cannot prevent me from leaving a stream.
16. As a PS TV player, I want all game inputs released when the controller disconnects, so that a character or camera does not remain stuck.
17. As a PS TV player, I want the stream to stay running during controller disconnection, so that I can reconnect without deliberately starting another cloud session.
18. As a PS TV player, I want input to resume only after the reconnected controller is neutral, so that held controls do not trigger accidental actions.
19. As a player, I want only one active controller to supply gameplay input, so that inputs from different controllers are not combined unexpectedly.
20. As a PS TV player, I want the guide to identify unsupported touchpad motion and other limitations, so that I know which games and controls I can use.
21. As a Vita player, I want handheld shoulders, sticks and touch controls to remain correct, so that PS TV support does not regress my accepted controls.

## Implementation Decisions

- Support DualShock 3 and DualShock 4 paired by the PS TV operating system. Provide concise system-pairing guidance; do not implement an app pairing screen or controller driver.
- Extend the platform controller adapter to identify the active supported PS TV controller and sample the appropriate system-assigned controller port. Do not assume the handheld port and legacy sampling expose all external inputs.
- Use the SDK's extended-controller capabilities where needed. Account explicitly for the different shoulder-button representation under extended sampling so handheld L/R remain L1/R1 and external L1/R1 remain distinct from L2/R2.
- Normalize supported physical controls into the existing gameplay input contract before applying the shared saved layout. Preserve both stick axes and trigger pressure where the system exposes it; verify actual trigger data on both controller types.
- Default physical game controls to their matching actions. DualShock 3 START/SELECT correspond to Options/Share; DualShock 4 OPTIONS/SHARE correspond to those actions using their own visible labels.
- Feed app menus through their existing navigation contract, with D-pad/left-stick behavior defined in #39. Gameplay remapping must never change app navigation.
- Integrate #37's one app-wide layout and input-combination rules. Physical L2/R2/L3/R3 must be real controller inputs, not dependent on unavailable handheld rear touch.
- Guarantee a cloud touchpad-click path through configurable ordinary-button remapping on either controller. Native DualShock 4 touchpad movement is excluded, and raw touchpad event availability must not be inferred from Sony's system touch-pointer feature.
- Keep the exit gesture physical and independent of mapping: hold START + SELECT for two seconds on DualShock 3 or OPTIONS + SHARE on DualShock 4. Suppress gameplay outputs derived from the chord inputs while it is held.
- Maintain one active controller and one owner of controller sampling. Do not merge multiple controllers into one player.
- Distinguish controller disconnection from a valid held state. On confirmed disconnection, submit a complete neutral game state promptly, including button/trigger/stick/touch releases, and retain the existing cloud session while it remains otherwise healthy.
- On reconnection, resume sampling the selected supported controller but gate gameplay output until controls are neutral, using the accepted stick-center/deadzone behavior. Do not replay buffered controls or infer an automatic Play/retry operation.
- Preserve existing session/transport error handling if the stream itself fails. Controller reconnection does not promise that a cloud service will retain a session indefinitely.
- Keep the accepted input polling ownership, feedback protocol and stop/join ordering. Update controller-specific help and feature claims only to the extent established by testing.
- Retain Vita defaults, front/rear touch behavior and media policies. PS TV work does not authorize video, FEC/reorder, display, audio or quality changes.

## Testing Decisions

- Exercise the existing controller-read-to-feedback boundary with representative DualShock 3, DualShock 4 and handheld samples, port/type information, connection changes and trigger values. Observe outgoing game controller states rather than private normalization helpers.
- Extend the existing input-worker adapter fixtures to represent extended inputs and connection state. Current fixtures only model legacy handheld data and cannot establish PS TV behavior.
- Cover every ordinary game input, simultaneous L1/L2/R1/R2, independent L3/R3, both sticks, trigger pressure, releases and correct START/SELECT versus OPTIONS/SHARE labeling/mapping.
- Cover shared saved mappings and Disabled from #37, many-to-one releases, configurable cloud touchpad click, and the unremappable physical exit chord.
- Verify disconnect while buttons, triggers and sticks are active: a complete neutral state is sent, the stream is not intentionally stopped, and no stale held state survives.
- Verify reconnect while controls are held, the required neutral observation, subsequent new input and repeated disconnect/reconnect. Do not mistake a transient empty sample for permission to replay stale state.
- Reuse app-flow tests for controller navigation and #39's D-pad integration. Reuse existing input-worker tests for neutralization, touch releases and stop/restart lifecycle.
- Perform real PS TV tests separately with a system-paired DualShock 3 and DualShock 4: controller port selection, full input matrix, trigger behavior, menu navigation, mapping, disconnect/reconnect, neutral gate and two-second exit.
- Perform a real Vita regression comparison against the accepted stream build, especially handheld shoulder bits, touch controls, releases, picture, audio and stop/restart.
- A successful host test or VPK build does not establish controller compatibility or an accepted stream build. Record hardware setup, observed limitations and the tested build identity before claiming support.

## Out of Scope

- PS2/DualShock 2 controllers, adapters, third-party controller plugins or unsupported controllers.
- App-managed Bluetooth/USB pairing or new controller drivers.
- Multiple simultaneous players/controllers, controller blending or multiplayer routing.
- Native DualShock 4 touchpad movement, motion controls or rumble.
- Per-controller saved profiles, per-game layouts or other remapping features beyond #37.
- Resuming a failed cloud session, guarantees about indefinite cloud-session retention, or changes to streaming media policies.

## Further Notes

The maintainer confirmed DualShock 3 plus DualShock 4 on 2026-11-02 and then confirmed the feature boundary, disconnect/reconnect behavior and test boundaries. This issue remains open for implementation.

GitHub already records this issue as blocked by #37; preserve that native dependency.

Coordinate with #37 for the shared layout/editor and remapped touchpad click, and #39 for menu navigation. Platform sampling can be developed independently, but full feature acceptance must exercise these integrations.

Sony lists both controller types and describes system pairing by USB followed by the PS button: [PS TV controller guide](https://manuals.playstation.net/document/en/pstv/basic/usecontroller.html). This establishes system-level support, not that the current app handles every input.

The [VitaSDK controller API](https://docs.vitasdk.org/group__SceCtrlUser.html) documents extended sampling, controller-port information and the shoulder-bit difference. Correct selected ports and real trigger behavior still need hardware verification. Sony's [touch-pointer guide](https://manuals.playstation.net/document/en/pstv/game/touchpointer.html) does not establish raw DualShock 4 touchpad delivery to this homebrew app.

Follow the existing architecture decision's single input owner, neutral/shutdown ordering and hardware acceptance requirements. The accepted stream build remains available as the comparison and rollback reference.

Original feedback:

> Controls for PS tv using ps2 controller.
> Is it something that can even be paired for ps4 controller?

The controller-generation ambiguity is resolved by the maintainer's confirmed scope above.
