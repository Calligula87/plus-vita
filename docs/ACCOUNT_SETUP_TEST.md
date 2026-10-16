# Account setup candidate: Vita hardware check

For [#15](https://github.com/Calligula87/plus-vita/issues/15), host tests use
synthetic credentials and simulated HTTP responses. The real app workflow,
file consumption, background worker, response parsers and cancellation execute
in those tests. They do not establish the on-device keyboard/display handoff,
physical controls or a live Sony sign-in. Those checks require the maintainer.

## Candidate and rollback

Use the specifically identified account-setup candidate supplied with the test
handoff, not an old VPK with the same generic build filename. Record its SHA256
and configuration. Installing it replaces the installed app because the title ID
is shared. Keep the accepted combined stream VPK for rollback; its identity is
in [the accepted milestone](PLAYABLE_MILESTONE.md).

There is no Remember option in this candidate. Import removes the token file
before sign-in, including successfully read malformed files. Manual entry also
cleans up a managed token file. Keep a private copy of your own credential off
the public repo if needed to restore your previous setup later. Never send
the credential, token file, browser response or a screenshot that shows it.

## Test steps

1. Launch and repeat the welcome/About/back/exit/touch checks in the
   [startup procedure](GRAPHICAL_STARTUP_TEST.md). There should be no token
   read, network request or diagnostic terminal just from opening the welcome.
2. Open Get started. Check that Sony sign-in/token retrieval instructions are
   readable. Continue to the file-transfer/manual-entry choices.
3. With no managed token file present, choose Import token file. Confirm the
   missing-file explanation and Back to setup.
4. Use a deliberately malformed synthetic file (no actual account material),
   such as `{ "npsso": "not-a-token" }`. Confirm that the file is removed and
   a format error appears without attempting authorization.
5. Open Enter token, cancel the Vita keyboard and confirm return to setup,
   restored graphical rendering and no replay of the cancel button. Reopen
   the keyboard and test a short synthetic token; it must be rejected before
   network authorization. Check that the field is masked.
6. Follow [the account setup guide](ACCOUNT_SETUP.md) with your own Sony
   account. Confirm the actual browser retrieval/file transfer instructions
   work. Import the valid file; check it has been removed with VitaShell.
7. Confirm authorization/catalogue progress remains responsive. On success,
   inspect the initial alphabetical catalogue and navigate across several
   pages using buttons/stick, then touch a row and continue with the controller.
   No Play action is connected in this candidate.
8. Return/exit and reopen. Confirm there is no silent persistent sign-in.
   Test the manual-entry path with your own token; the keyboard should return
   cleanly to the graphical progress/catalogue screen.
9. Cancel a real authorization/catalogue load with Circle/Cancel. Confirm
   immediate return to welcome, and no late catalogue opening. A retry while
   the old worker finishes may explain that it is busy; retry after it settles.
10. With Wi-Fi unavailable, check readable failure and working Back/exit.
    Do not describe this as the full sleep/wake/recovery feature in #19.

Record pass/fail/not-tested for each relevant step, whether picture/text/focus
or keyboard handoff fails, and whether both real import routes reach catalogue.
Report only the observed symptom and step number, never secret values.

The gate remains open if a live sign-in, transfer instructions or keyboard
handoff is unverified. Downstream #16/#17 remain blocked by #15 until that gate
is resolved; code/build success is not a substitute for it. The final 45-minute
beta stream acceptance is separate work in #23.
