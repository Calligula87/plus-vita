# Connect your own Sony account

This describes the account-setup development candidate for
[#15](https://github.com/Calligula87/plus-vita/issues/15). It reaches a basic
alphabetical catalogue; the complete beta's covers, settings and Play flow are
subsequent tickets. The actual Vita setup procedure still needs hardware
validation; simulated responses are not proof of live Sony compatibility.

## Get a sign-in token on your phone or PC

1. Use a browser to sign in to your own account at
   [PlayStation](https://www.playstation.com/). Complete Sony's password,
   [passkey](https://www.playstation.com/en-us/support/account/set-up-passkey/)
   or [two-step verification](https://www.playstation.com/en-us/support/account/2sv-login/)
   prompts on that device.
2. In the **same browser/profile**, open Sony's
   [sign-in cookie endpoint](https://ca.account.sony.com/api/v1/ssocookie).
   The browser should show JSON containing an `npsso` field. If it shows an
   error or has no such field, sign in again in that browser and retry.
   This endpoint route is also used by the pinned upstream Pylux sign-in flow;
   the complete Plus Vita recipe must still be checked on the candidate.
3. Copy only the `npsso` value: 64 letters and numbers, without surrounding
   quotes. Treat it as an account-access secret. Do not post the browser
   response, token, token-file contents or a screenshot containing it to GitHub.

Plus Vita does not ask for your Sony email/password and does not receive your
passkey or verification code. Sign-in stays with Sony; the transferred token
authorizes the Vita application.

## Import a file with VitaShell

1. Create a UTF-8 plain-text file without a byte-order mark, named
   `plus-vita-auth.json`, containing only this JSON shape:

   ```json
   {"npsso":"REPLACE_THIS_WITH_YOUR_64_CHARACTER_TOKEN"}
   ```

   Replace the placeholder with your token. Do not include other fields or
   extra text. The placeholder above is deliberately not a valid credential.
2. Use your usual VitaShell file-transfer method to place the file at
   `ux0:data/plus-vita-auth.json` on the Vita.
3. Open Plus Vita, choose **Get started**, read the sign-in instructions and
   choose **Continue**, then **Import token file**.
4. The app reads and removes the imported file before authorizing. The current
   candidate is **session only**: there is no Remember switch yet. Keep a
   private copy on your own phone/PC if you will need to import it again after
   exiting. Never put this private copy inside a public repo or issue report.
5. If removal fails, the app stops before sign-in and tells you that cleanup
   failed. Check/remove the file with VitaShell and try again. A malformed file
   that was read successfully is also removed, then an error explains how to
   replace it.
6. While authorization/catalogue loading runs, Circle or **Cancel** returns to
   the welcome screen. Network work is cancelled cooperatively; an in-progress
   DNS/OS operation may finish before the worker exits. If you immediately try
   again while that happens, the app asks you to wait briefly. A late result
   cannot reopen the cancelled account.

The app's file removal is normal filesystem deletion, not a guarantee that
storage blocks cannot be recovered. This candidate does not claim protected
persistent storage.

## Enter the token on Vita instead

Choose **Enter token** at setup. Enter the same 64-character token with the
Vita keyboard, without JSON, quotes or spaces. The token field is masked.
Cancel returns to setup; Enter continues to the same session-only authorization
and catalogue flow. This route also removes any managed import file before
authorization, so an earlier file is not silently retained.

## Read the result

- Successful authorization opens the initial graphical catalogue. Use the
  D-pad or left stick to browse, or touch a visible row to select it.
- A rejected/expired token asks you to sign in again on your phone/PC and obtain
  a fresh token. The Sony password or a Vita system device password is not an
  NPSSO replacement.
- Missing files, format errors, cleanup failures, network errors, unavailable
  catalogue and an empty catalogue have separate readable outcomes.
- This candidate cannot launch games from the catalogue yet. Do not interpret
  a listed title as a guarantee of streaming eligibility.
- Back/exit clears the active runtime account; reopening requires another
  import or manual entry. Remember/Forget controls follow in #16.

For the hardware check, follow [the candidate procedure](ACCOUNT_SETUP_TEST.md).
Do not attach this credential file or a full app-data folder to an issue.
