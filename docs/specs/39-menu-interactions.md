# Improve menu navigation with D-pad support, touch release and short transitions

Approved spec for [GitHub issue #39](https://github.com/Calligula87/plus-vita/issues/39). Published to GitHub on 2026-11-02.

## Problem Statement

Plus-Vita menus change screens immediately when a finger first touches a target. A player can still have their finger on the screen when the next screen appears, making the interface feel abrupt and giving little visible confirmation of the press.

Players also expect the D-pad to navigate games and app menus, but the current graphical interface deliberately accepts only the left stick for directional navigation.

## Solution

Add D-pad navigation alongside the left stick throughout app menus and the game catalogue. Give menu touch targets an immediate pressed state, activate only on release inside the original target, and cancel the gesture when the finger slides away.

Use short menu-screen transitions of approximately 100–150 ms. Each gesture performs at most one action, and input belonging to an old screen cannot activate controls on the next screen. Keep Cross selection and Circle back consistent.

## User Stories

1. As a player, I want to navigate every app menu with the D-pad, so that navigation matches my controller expectations.
2. As a player, I want to browse the game catalogue with the D-pad, so that I do not need the left stick to select a game.
3. As a player, I want holding a D-pad direction to repeat navigation, so that I can browse longer lists efficiently.
4. As a player, I want D-pad and left-stick navigation to share focus and scrolling behavior, so that switching between them feels consistent.
5. As a player, I want the left stick to remain available, so that I can continue using my preferred navigation method.
6. As a player, I want Cross to select and Circle to go back, so that existing menu controls stay familiar.
7. As a player, I want a touched button to show a pressed state immediately, so that I know the touch was recognized.
8. As a player, I want a touch action to activate when I lift my finger inside its original target, so that the screen does not change underneath my finger.
9. As a player, I want sliding away from a target to cancel the gesture, so that I can avoid an accidental selection.
10. As a player, I want touching outside an actionable target to do nothing, so that background touches do not change screens.
11. As a player, I want one touch gesture to perform at most one action, so that a single tap cannot make several selections.
12. As a player, I want screen changes to have a short transition, so that the interface communicates the change clearly.
13. As a player, I want a transition to finish promptly, so that feedback does not make navigation feel slow.
14. As a player, I want an old touch or held activation button to be consumed across a screen change, so that it cannot select a new screen's control accidentally.
15. As a player, I want cancelled or interrupted gestures to release their pressed appearance, so that the interface never looks stuck.
16. As a player, I want catalogue search, details, back navigation and scrolling to keep working, so that the interaction improvements do not disrupt browsing.
17. As a player using the mapping editor, I want the same navigation and touch rules there, so that new Settings screens remain consistent.
18. As a PS TV player, I want supported controller directions to use the same menu behavior, so that the app works without handheld touch.
19. As a player, I want gameplay controls and streaming response preserved, so that menu animation does not change the playing experience.

## Implementation Decisions

- Extend the existing app navigation contract to recognize D-pad directions alongside the left stick. Use the same focus ordering, catalogue navigation and boundary/scrolling behavior; do not build a separate navigation model.
- Retain the existing initial hold delay and repeat cadence, currently 400 ms then 160 ms. Resolve simultaneous directional sources through one deterministic navigation/repeat decision so a D-pad and stick do not double-step focus.
- Apply directional navigation consistently across app-owned menus, including the new mapping editor. Cross selects, Circle goes back and existing search controls remain available.
- Replace menu activation on touch-down with a complete gesture lifecycle: touch-down captures the original actionable target and displays its pressed state; movement outside that target cancels the gesture; touch-up activates only an uncancelled gesture released inside the same target.
- A cancelled gesture stays cancelled until a new touch starts. Touch-down on background does not become an action merely by moving onto a button. Extra contacts must not create extra activations.
- Tie capture to the original target and screen/view context. Navigation, asynchronous screen replacement, interruption, splash dismissal or transition must invalidate stale capture and release the visual pressed state.
- One gesture produces at most one activation. Consume input responsible for changing screens; neither a held contact nor the release of an old gesture may activate an action on the destination screen.
- Extend the existing UI-flow event boundary to carry touch down/move/up/cancel information. Keep hardware coordinate/contact sampling in the platform adapter, but put gesture behavior at the highest practical app/UI-flow seam so host tests can exercise the real policy.
- Add a short, time-driven menu-screen transition of approximately 100–150 ms with no blocking sleep. Keep immediate pressed feedback and suppress duplicate/stale activations during the transition instead of queuing them for the next screen.
- Keep transition work bounded and confined to menu presentation. Do not change gameplay touch mapping, controller sampling, stream policies or the accepted opening artwork/branding.
- Update the controller guide and feature descriptions to include D-pad menu navigation and the touch-release behavior.

## Testing Decisions

- Use the existing app/UI-flow journey as the primary seam. Feed physical-style pad samples, touch down/move/up/cancel sequences and controlled time into the real flow; observe focus, current screen, visible pressed state, generated effects and transition completion.
- Extend the existing workflow/catalogue tests for navigation and the branding/UI-flow tests for timed presentation and held-input suppression. The old assertion that D-pad is ignored must be replaced with the agreed behavior.
- Do not rely solely on directly injecting a final activation or synthetic completed tap: that would miss the finger-down versus finger-up defect and the platform-to-flow lifecycle.
- Cover D-pad taps, initial/repeated holds, release and direction changes, catalogue scrolling/boundaries, left-stick compatibility and combined inputs without double navigation.
- Cover press feedback before release, release inside the original target, release outside, slide-away cancellation, leaving and returning before release, background touches, held touches and extra contacts. Each gesture must produce zero or one action as appropriate.
- Cover screen replacement, asynchronous completion, splash dismissal, cancellation and transition while input is held. No stale touch or held activation button may activate a destination-screen target.
- Use a controlled clock for the short transition. Verify visible start/completion and interaction safety without wall-clock sleeps or assertions tied to private animation fields.
- Recheck search, catalogue/details/back, Settings, confirmation screens and the mapping editor when #37 is integrated. Retain existing app recovery and effect-cancellation behavior.
- Perform real Vita checks for touch feel, visible pressed feedback, slide-away cancellation, transition smoothness, D-pad/stick repeat and stop/relaunch. Compare picture, audio, controls and stability against the accepted stream build.
- When combined with #38, verify D-pad/stick, Cross and Circle on PS TV with both controller types. Host success does not establish hardware acceptance.

## Out of Scope

- Gameplay remapping or changes to game input response; those bindings belong to #37.
- PS TV pairing, controller discovery or extended gameplay sampling; those belong to #38.
- A visual redesign, new branding/artwork, long cinematic transitions or additional gameplay animations.
- Changing stick sensitivity, the established repeat cadence, streaming media policy or gameplay touch semantics.

## Further Notes

The maintainer confirmed the interaction scope and highest-level test boundaries on 2026-11-02. This issue replaces the original feedback with the agreed implementation spec and remains open for implementation.

The current D-pad exclusion and touch-down activation are deliberate existing behavior, so their documented contracts and tests must change together. This is a menu interaction change, not a latency experiment.

Coordinate menu integration with #37 and #38 while preserving the accepted stream build and existing hardware acceptance requirements.

Original feedback:

> the UI doesn’t have any animation so it “feels” a little off putting when you tap something and it instantly reacts so fast that your finger is still there and the entire screen already changed.
>
> The D pad also won’t scroll games, can only use the thumb stick for that which also felt weird.
