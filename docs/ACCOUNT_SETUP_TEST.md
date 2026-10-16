# Updated UI/account candidate: Vita hardware check

This procedure covers the feedback revisions to
[#14](https://github.com/Calligula87/plus-vita/issues/14) and
[#15](https://github.com/Calligula87/plus-vita/issues/15), specified in
[#25](https://github.com/Calligula87/plus-vita/issues/25).

The earlier candidate successfully reached the graphical catalogue using
file-based authorization, as reported by the maintainer. Circle then prevented
re-entry after the import was consumed. Host regressions now cover re-entry
with the file absent, About/cancelled exit, joystick input, JSON-response formats,
file cleanup, real worker/parsers and cancelled transport. They do not establish
that the revised visuals, physical controls or saved-response guide work on Vita.

## Candidate and rollback

Use the uniquely named feedback candidate and SHA256 supplied with the test
handoff, with Release, graphical app, input worker ON and deferred log ON.
Installing replaces the installed app because the title ID is shared. Keep
the accepted combined stream VPK for rollback; its identity is in
[the accepted milestone](PLAYABLE_MILESTONE.md).

Import removes the transferred JSON, including readable malformed files.
Keep a private copy on your phone/PC if needed after exit. Do not send the
file, browser response, secret-bearing screenshot or full app-data folder.
This candidate has no Remember or Play action; those are subsequent tickets.

## Test steps

1. Launch: check the blue background, gold focus/accent, readable text and
   neutral APP LOGO placeholder. The former console illustration and cyan
   should be gone. Check for clipping, flicker or black screens.
2. Move through welcome/About/exit using the **left joystick**, Cross and
   Circle. Check neutral deadzone, both directions, initial repeat delay and
   steady held-stick repeat. Press each D-pad direction: menu focus must not
   change. Holding Cross/Circle must not repeatedly activate/back out.
3. Tap visible buttons; hold/release touch and check that one tap does not
   activate a second screen. About and cancelled Exit should return cleanly.
4. Open Get started. Check the short save-response/transfer guide. Setup has
   only **Import JSON** and **Back**; there should be no Enter token action.
   With no import file present, confirm the explanation tells you to transfer
   the JSON, and Back returns to setup.
5. Use a deliberately invalid synthetic file, such as `{ "npsso": "invalid" }`.
   Confirm it is removed and a fixed error appears without account access.
6. Follow [the revised setup guide](ACCOUNT_SETUP.md): sign in to Sony on your
   phone/PC, save the actual JSON response, transfer it under the expected
   name using VitaShell and import. Confirm it reaches the alphabetical
   catalogue and the transferred JSON is gone. Record any save/download step
   that does not work; do not share the file or its contents.
7. Select a game on a later catalogue page. Press Circle: the home should say
   the account is connected and offer **Open catalogue**. Open it: the selected
   game should be retained, without file import or another sign-in. Repeat
   several times. Repeat after About and after cancelling Exit, using both
   joystick/Cross and touch to reopen the catalogue.
8. Confirm Exit and reopen the app. This candidate is session-only: Get
   started appears and importing again requires a fresh transfer. Re-entry
   during one app run must not be confused with remembering across launches.
9. On a new setup attempt, cancel authorization/catalogue loading with Circle
   or Cancel. Confirm immediate return to welcome and no late catalogue opening.
   File consumption still completes if already started. Transfer another JSON
   copy to retry; if the old worker is busy, wait briefly and try again.
10. With Wi-Fi unavailable, check a fixed failure and working Back/exit.
    This is not the full #19 recovery/sleep feature.

Report step numbers as pass/fail/not-tested and describe the visible symptom.
Do not infer a full control, storage, error-path or streaming pass from one
successful import. The revised #14/#15 gate remains open pending these checks;
#16/#17 depend on #15. Final 45-minute beta stream acceptance is separate in #23.
