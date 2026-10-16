# Graphical startup check

Issue [#14](https://github.com/Calligula87/plus-vita/issues/14) adds the first
graphical startup slice. This is a startup preview, not the complete beta.
Account import and streaming remain subsequent tickets.

## Build and automated validation

Run the actual native workflow tests on a host with a C compiler:

```sh
sh tools/test-app-workflow.sh
```

The tests exercise welcome focus, button navigation, About/back, deliberate
exit, touch hit areas and operation cancellation/late results. They do not
establish Vita font rendering, physical input or display ownership behavior.

With VitaSDK installed, use a separate build directory to preserve the accepted
stream package and its configuration:

```sh
cmake -S vita-client -B build/vita-ui -DCMAKE_BUILD_TYPE=Release \
  -DPLUS_VITA_INPUT_WORKER=ON -DPLUS_VITA_DEFERRED_LOG=ON
cmake --build build/vita-ui --parallel
```

The normal entry point is graphical. To reproduce the historical diagnostic
interface, additionally configure `-DPLUS_VITA_DIAGNOSTIC_APP=ON` in a separate
directory. This does not edit or replace a retained accepted VPK.

## Vita procedure

Record the exact candidate checksum and build options before installation.
The application ID is shared with the accepted stream app, so installation
replaces the installed app. Retain its accepted VPK for rollback. Never include
credential files or personal data in a test report.

1. Open the app. Verify that the dark graphical welcome appears promptly,
   with Get started selected and readable title, body and button hints.
   There should be no NPSSO prompt, network request, diagnostic self-tests or
   terminal output during this startup slice.
2. Navigate using the D-pad and left stick in both directions. Check focus,
   stick dead zone and repeat timing. Holding X must not repeatedly activate.
3. Open About and return using Circle and the visible Back button. Verify that
   the welcome selection is restored.
4. Tap visible controls. Taps outside their bounds should do nothing; holding
   a touch must not activate another screen repeatedly. Check screen edges.
5. Open Get started. Verify the preparation/setup guide and cancel/back flow.
   The preview accurately states that account import is not included yet.
6. Select Exit or press Circle on welcome. Stay here is selected first. Circle
   cancels; explicitly choosing Exit closes the app. Reopen and repeat.
7. Check text and buttons for clipping, overlap and flicker, and check that the
   app returns to the system cleanly. Report any black screen or input issue.

Record each result as pass/fail/not tested. A host pass and generated VPK do
not constitute a hardware pass or acceptance of a new stream build. Final beta
acceptance remains [#23](https://github.com/Calligula87/plus-vita/issues/23).
