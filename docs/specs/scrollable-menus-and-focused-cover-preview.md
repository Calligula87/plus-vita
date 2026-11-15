# Scrollable app menus and a lightweight catalogue with focused cover previews

Published spec: [https://github.com/Calligula87/plus-vita/issues/40](https://github.com/Calligula87/plus-vita/issues/40). The interaction decisions and three-ticket breakdown are approved.

## Problem Statement

Players must repeatedly use Previous/Next controls to browse long control-mapping menus, while the catalogue presents only a few large game cards at a time. This makes browsing feel simplistic and interrupts movement through a continuous set of choices.

The catalogue also requests artwork for every visible game card immediately. Players who are moving quickly through titles do not need those covers yet. They want a readable title list, a clear indication of scroll position, and one reserved preview area that loads the highlighted game's cover after they pause.

## Solution

Use a shared list style throughout app-owned menus, preserving the blue/gold branding. Lists that exceed the available space scroll continuously and show a proportional side scrollbar. Short lists show all items. Remove Previous/Next menu controls and stop directional navigation at the first and last item.

The catalogue shows roughly six to eight readable title rows on the left and one fixed cover area on the right, with Search and Back outside the game list. Highlighting a game starts a three-second preview countdown. Show "Loading cover" during the wait, then reuse cached artwork or request only that game's cover. Navigation remains available throughout.

D-pad and stick navigation retain hold-to-repeat. Touch supports dragging without momentum; dragging cancels taps. A first tap on a game highlights it, and a second tap on that highlighted row opens details. Cross opens the highlighted game's details immediately.

## User Stories

1. As a player, I want app-owned menus to use a consistent list style, so that navigation feels familiar across screens.
2. As a player, I want long menus to scroll continuously, so that I can reach choices without Previous/Next controls.
3. As a player, I want short menus to show all their items, so that they remain quick to use.
4. As a player, I want an overflowing list to show a side scrollbar, so that I understand where I am within it.
5. As a player, I want the scrollbar thumb to reflect the visible portion and current position, so that I can judge how much remains.
6. As a player, I want navigation to stop at the first and last item, so that reaching an edge does not unexpectedly move me elsewhere.
7. As a player, I want D-pad and left-stick navigation with hold-to-repeat, so that long lists remain usable with physical controls.
8. As a touch user, I want to drag a list to scroll it, so that I can browse directly on the screen.
9. As a touch user, I want scrolling to stop when I release my finger, so that I retain control of the position.
10. As a touch user, I want a drag to cancel its pending tap, so that scrolling cannot activate an item.
11. As a player, I want focused items to remain visible during controller navigation, so that I can always see my current choice.
12. As a player editing controls, I want to browse all input bindings in one list, so that I can find physical and rear-touch inputs without paging.
13. As a player editing controls, I want one scrolling action picker, so that I can find a destination or Disabled without paging.
14. As a player editing controls, I want to return to the input I was editing, so that repeated edits do not send me back to the top.
15. As a player editing controls, I want Save, Cancel and Restore defaults to retain their existing meaning, so that browsing changes do not alter my saved layout unexpectedly.
16. As a player, I want the catalogue to show a readable title list, so that I can see more games at once.
17. As a player, I want long row titles to use an ellipsis and details to show the full title, so that the list stays readable.
18. As a player, I want a dedicated cover area on the right, so that cover loading never shifts the title list.
19. As a player, I want Search and Back outside the scrolling game rows, so that those actions remain easy to reach.
20. As a player, I want Triangle and Circle shortcuts to remain available, so that I can search or go back from any game row.
21. As a player, I want a three-second pause on the highlighted game before its catalogue cover appears or starts downloading, so that fast browsing avoids unnecessary artwork work.
22. As a player, I want the preview to show "Loading cover" during that countdown and any subsequent transfer, so that I understand its current state.
23. As a player, I want changing the highlighted game to clear the old image and restart the countdown, so that an old cover is never presented as the new selection.
24. As a touch user, I want dragging to suspend the preview countdown until release, so that artwork does not load while I am scrolling.
25. As a player, I want previously cached covers reused after the countdown, so that revisiting games does not cause redundant downloads.
26. As a player, I want only the selected game's cover requested, so that neighbouring rows do not cause artwork prefetches.
27. As a player, I want obsolete artwork work cancelled and late results ignored for display, so that browsing does not show the wrong game.
28. As a player, I want missing artwork to say "No cover" and failed artwork to say "Cover unavailable", so that the outcome is clear.
29. As a player, I want to retry failed artwork by moving away and highlighting the game again for three seconds, so that a temporary failure does not require restarting the app.
30. As a player, I want artwork failures to leave navigation, details and Play usable, so that missing artwork cannot prevent gaming.
31. As a touch user, I want a first game-row tap to highlight and a second tap to open details, so that I can preview a game without entering another screen.
32. As a controller user, I want Cross to open details immediately, so that preview timing never delays an explicit selection.
33. As a player opening details, I want its cover to load immediately or reuse the same cached image, so that an explicit choice does not have to wait for the browsing countdown.
34. As a player returning from details or Play, I want my filter, highlighted game and scroll position restored, so that I can continue browsing where I left off.
35. As a player, I want search, cancelled search, empty catalogues and zero-result states to work with the new list, so that browsing remains complete.
36. As a player, I want stale touch gestures cancelled across scrolling, screen replacement, sleep and transitions, so that an old input cannot activate a new target.
37. As a PS TV player, I want the same list behavior with supported controllers, so that the interface is usable without touch.
38. As a player, I want the accepted gameplay experience preserved while menus change, so that the UI work does not change streaming quality or gameplay controls.

## Implementation Decisions

- Use the existing app dispatcher, UI flow and painter as the shared interaction boundary. Extend the public view to describe a list's total items, viewport, highlighted item and visible rows instead of exposing only a page of actions.
- Keep item identity and focus independent of visible-row index. Use a bounded viewport and prepare/render visible rows rather than building or painting every catalogue item on each frame.
- Introduce the shared scrolling behavior through the complete control-mapping journey first. Existing app screens continue to work during migration; ordinary menus and the catalogue then adopt the same contract.
- Render the existing blue/gold identity with compact list rows and a clear highlight. Show a thin proportional scrollbar only when the list overflows. Its first version is a position indicator; touch scrolling occurs on the list itself.
- Clamp directional navigation at list boundaries. Retain the accepted directional dead zones and initial/subsequent repeat cadence, using one navigation decision for combined input sources.
- Support single-contact drag scrolling without momentum. Distinguish a tap from a drag using a documented movement threshold; a drag, extra/lost contact, interruption or screen replacement must cancel activation. Preserve the existing short screen-transition input barrier.
- During touch scrolling, keep the highlighted item visible or move focus to the nearest visible row when it would otherwise leave the viewport. No activation is produced by that focus adjustment. On release, the resulting highlight is the starting point for controller navigation and preview timing.
- Preserve mapping draft/save/cancel/defaults, source availability labels, bindings and fixed physical exit behavior. Selecting an action or returning from its picker restores the edited input's focus and viewport.
- Catalogue layout targets roughly six to eight readable rows on the left, with a fixed right preview region. Keep the existing bounded decoded cover size, ellipsize long row titles, and retain the full title in details.
- Keep Search and Back separate from catalogue game rows. Preserve Triangle/Circle shortcuts, current title ordering/filter semantics, search cancellation and useful empty/error states.
- Treat "selected for three seconds" as the highlighted catalogue game. Use monotonic time and a game/account identity, not just a row position or cover URL.
- On every newly highlighted game, clear the old preview and show "Loading cover". Require at least 3,000 ms of uninterrupted settled focus before displaying cached artwork or becoming eligible to start a transfer. Dragging suspends the countdown, and release starts a fresh settled interval.
- Cover loading and decoding stay on the existing background worker. At eligibility, request only the highlighted game's URL; no neighbouring-row prefetch. A worker still finishing cooperative cancellation may delay the actual transfer beyond the eligibility time, but must never block navigation.
- Retain the bounded six-cover URL cache, single worker and existing network/decode limits. This change reduces artwork request activity during browsing; it does not itself establish lower total memory use or measured Vita responsiveness.
- Cancel obsolete desired artwork when focus changes, dragging begins, the catalogue loses its selected game, or its account context changes. Late output must never appear for a different game/account; preserve generation checks and account-reset behavior.
- Known absent artwork can display "No cover" immediately. Transfer/decode failure displays "Cover unavailable" without a continuous retry loop. A new visit to that title, followed by a fresh three-second dwell, permits one retry.
- Details is an explicit user choice and may request its cover immediately, sharing the existing cache. Returning restores catalogue focus and viewport. A loaded preview for the same retained highlight can be reused on return; moving to a different highlight always starts a new three-second interval.
- Apply or clear a search to establish the new result list and a valid visible highlight. Preserve existing cancelled-search behavior. A newly established game highlight follows the same dwell rule.
- Keep artwork scheduling policy inside the testable shared app/UI flow. The Vita adapter supplies input/time, runs effects and presents results rather than maintaining a second selection/countdown policy.
- The implementation is divided into three complete tickets: shared scrolling through control mapping; adoption by ordinary menus; and the lightweight catalogue with its complete cover lifecycle. Ordinary menus and catalogue depend on the first ticket, not on each other.

## Testing Decisions

- Use one primary behavior seam: the real app/UI flow and painter, extended to the real cover worker with simulated external HTTP/device/font boundaries and a controlled clock. Assert visible rows, focus, scrollbar position, activation/effects and actual transfer starts, not private list helpers or timer fields.
- Reuse existing menu workflow, touch/transition and painter journeys for taps, drags, controller repeats, clamped boundaries, stale gestures, sleep and focus restoration.
- Reuse existing control editor persistence/gameplay integration to prove that scrolling does not change draft/save/cancel/defaults or mapped outputs.
- Reuse the real catalogue/account/cover integration and its HTTP request counter for artwork behavior. Ensure the production scheduler is exercised, rather than manually requesting covers only inside tests.
- Cover zero, one, exactly-fitting and overflowing lists; first/last positions; long titles; filtered results; drag cancellation; controller/touch transitions; and return from nested screens.
- Prove no transfer starts during rapid highlight changes or active dragging. Check the three-second boundary, cached display after the same delay, one eligible selected URL, cancellation, late completion, retry after a new visit and account reset.
- Verify that explicit details loading can start immediately, cached artwork is shared, and navigation/Play remain usable during artwork failure or cancellation.
- Inspect actual-font previews for representative short menus, long mapping lists, catalogue selection, scrollbar extremes and placeholder/error states.
- Run relevant host tests and Vita builds. Compare the new candidate with the accepted stream build on real Vita; verify controller-only use on PS TV. Record the tested build and exact hardware observations before calling it an accepted stream build.
- Measure any claimed browsing improvement on hardware. Controlled request counts prove reduced artwork activity; they do not measure physical input response latency or total process memory.

## Out of Scope

- New branding/artwork, large animation systems, scrolling momentum or a draggable scrollbar thumb.
- Changes to Sony's system keyboard, pairing dialogs or other operating-system-owned UI.
- Changing catalogue data sources, sorting/search semantics, account/authentication or cloud eligibility.
- Background cover prefetch, an expanded/unbounded cache, larger decoded artwork or speculative cache-memory optimization.
- Changes to game streaming video, FEC/reorder, display/audio policy, stream quality, gameplay mappings or the fixed exit shortcut.
- New controller support, per-game/controller layouts or other features beyond the existing control work.
- Publication of a new release or hardware acceptance based only on host tests.

## Further Notes

The maintainer confirmed both design rounds, the test boundary and the three-ticket breakdown in this conversation. Implementation is tracked through the native child issues: the mapping-list ticket starts first; ordinary menus and catalogue then depend on it and can proceed independently.

This follows the existing #37/#38/#39 implementation, whose earlier hardware acceptance gates remain separate. The current local source baseline is c19cbdb. Existing accepted/fallback packages must remain available for comparison.

Most ordinary menus already fit on one screen. Their adoption mainly provides a consistent list presentation and correct overflow behavior; mapping and catalogue lists are the main scrolling journeys.

The existing loader already caches covers and serializes downloads. The expected reduction comes from requesting only a settled highlighted game's cover and avoiding visible-neighbour requests, not from replacing simultaneous downloads that the current code never performs.
