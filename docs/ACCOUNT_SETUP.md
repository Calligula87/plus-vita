# Connect your own Sony account

The account-setup development candidate for
[#15](https://github.com/Calligula87/plus-vita/issues/15) opens a basic
alphabetical catalogue. Covers, search, settings, Remember and Play follow in
later tickets. The maintainer reached the catalogue using file import on the
first candidate. The revised saved-response instructions and account re-entry
need a new [Vita check](ACCOUNT_SETUP_TEST.md).

## Save your sign-in JSON on a phone or PC

1. Sign in to your own account at [PlayStation](https://www.playstation.com/).
   Complete Sony's password, passkey or verification prompts on that device.
2. In the **same browser/profile**, open the
   [Sony sign-in response](https://ca.account.sony.com/api/v1/ssocookie).
   It should show JSON containing an `npsso` field. If it shows an error or no
   such field, sign in again in that browser and retry.
3. Use the browser's save/download action to save that response as
   **plus-vita-auth.json**. On a PC, use Save as (usually Ctrl+S); keep JSON
   format and the `.json` filename. Save the response itself, not an HTML page
   or a screenshot. If your phone cannot save the response, use a PC browser.

Keep this file private: it grants account access. Do not post it, the browser
response or screenshots showing its contents to GitHub. You do not need to
extract or type the token, construct JSON or remove other response fields.

Plus Vita does not ask for your Sony email/password, passkey or verification
code. Sony handles sign-in. The transferred JSON supplies the Vita credential.

## Transfer and import

1. Transfer **plus-vita-auth.json** with VitaShell to
   **ux0:data/plus-vita-auth.json**. Keep a private copy on your phone/PC if
   you will want to sign in again after exiting the app.
2. Open Plus Vita, choose **Get started**, then **Continue** and **Import JSON**.
3. The app reads and removes the transferred file before authorization. It
   accepts a UTF-8 JSON response up to 4096 bytes, including a UTF-8 byte-order
   mark. Unrelated response fields are ignored. Invalid or ambiguous credential
   data, malformed JSON and oversized files are rejected without displaying
   their contents. Readable malformed files are also consumed and removed.
4. If cleanup fails, sign-in stops with a fixed explanation. Remove the file
   using VitaShell and transfer a fresh copy before trying again.

There is no Remember switch in this candidate. Import stores your account
**only in memory for the current app run**. Normal filesystem deletion is not
a guarantee that old storage blocks cannot be recovered. No protected
persistent storage is claimed.

## Keep browsing during this app run

- Successful authorization opens the catalogue. Browse with the **left
  joystick**, or touch a visible row. D-pad presses do not move app menu focus;
  this preference does not change controls inside streaming games.
- **Circle** returns to the connected home. Choose **Open catalogue** to return
  immediately with the selection retained. The imported file is no longer
  needed. About and cancelling Exit keep the account connected too.
- Confirming Exit ends the app run. On the next launch, transfer/import your
  JSON again. Remember and Forget controls follow in #16.
- During initial import/authorization, **Circle** or **Cancel** cancels setup.
  This does not establish a connected account. File consumption still finishes
  if already started. If it was consumed, transfer a new copy before retrying.
  Network cancellation is cooperative; an in-progress DNS/OS call may finish
  before the worker exits. If it is busy, wait briefly before retrying. A late
  result cannot open a cancelled account.
- Missing, invalid, expired/rejected credentials, cleanup failure, connectivity
  failure, unavailable catalogue and empty catalogue have readable outcomes.
  For expired/rejected sign-in, save a fresh browser JSON response and import it.
- This candidate has no Play action yet. A listed game does not guarantee
  streaming eligibility.

Follow the [candidate test procedure](ACCOUNT_SETUP_TEST.md). For reports,
provide step numbers and symptoms; never include credential files or a full
app-data folder.
