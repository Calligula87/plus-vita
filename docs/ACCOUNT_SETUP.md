# Connect your own Sony account

## Save your sign-in JSON on a phone or PC

1. Sign in to your own account at [PlayStation](https://www.playstation.com/).
   Complete Sony's password, passkey or verification prompts on that device.
2. In the same browser/profile, open the
   [Sony sign-in response](https://ca.account.sony.com/api/v1/ssocookie).
   It should show JSON containing an `npsso` field. If not, sign in again and retry.
3. Save the response itself as **plus-vita-auth.json**, using the browser's
   save/download action (usually Ctrl+S on a PC). Save JSON, not an HTML page or
   screenshot. You do not need to extract or type the token or edit the response.
4. Transfer it with VitaShell to **ux0:data/plus-vita-auth.json**.

Keep this file private: it grants account access. Never post it, the browser
response or screenshots of its contents to GitHub. Plus Vita does not ask for
your Sony password, passkey or verification code; Sony handles sign-in.

## Open the app

The app automatically checks for an existing saved account, then uses your
JSON if no saved account exists. It validates the credential before opening
the catalogue. **The original JSON stays in place**, unchanged, including after
manual import, cancellation or a malformed response. Successive launches can
read it again while Sony accepts the credential.

If the app is already open, choose Get started → Continue → Import JSON.
Remember defaults On for fresh setup: placing the JSON in the folder enables
automatic sign-in. It accepts UTF-8 JSON up to 4096 bytes, including a byte-order
mark; unrelated response fields are ignored and invalid/ambiguous credentials
are rejected without showing their contents.

Account data is locally readable with file access. The original browser response
remains your file. The app may separately save only the needed NPSSO and optional
user-entered label; it does not claim an encrypted credential vault. The label
is not a Sony-verified identity.

Account headings use the authenticated Sony `onlineId` when available. A local
label entered in Account overrides it and is shown as a user-entered label.
Before authorization completes, a saved local label may appear; otherwise the
heading stays generic. The Sony name is runtime-only and is not saved alongside
the credential or written to diagnostics.

## Account controls and recovery

- Circle returns to connected home. Open catalogue, About and cancelled Exit
  retain the runtime account without rereading the file. Use the left joystick,
  Cross/Circle or touch; the D-pad does not navigate app menus.
- Account → Remember Off removes the app-managed saved copy and pauses automatic
  sign-in, including reuse of the retained JSON. The current runtime account
  remains usable. Remember On resumes persistence; the JSON stays in place.
- Forget clears runtime sign-in, saved account/label and catalogue state, then
  returns to setup. It keeps your JSON but blocks automatic reuse. Choose Import
  JSON explicitly to connect again; enable Remember if you want automatic sign-in.
- Confirmed rejection/expiry also blocks automatic reuse. Replace the JSON with
  a fresh browser response, then explicitly import it. No fixed token lifetime
  or automatic renewal is promised. Wi-Fi, rate-limit and service failures retain
  the credential and offer Retry without requiring another transfer.
- Missing, unreadable or malformed files produce fixed readable errors. Replace
  malformed files using VitaShell; the app does not delete them. Storage write
  and cleanup failures offer recovery and do not claim success.
- Cancelled or late account results cannot reconnect a forgotten account.
  Network cancellation is cooperative; allow an existing worker to finish before
  retrying if it is still busy.

The development catalogue shows alphabetical cover tiles. Browse with the left
stick (left/right or up/down), open details with Cross or touch, and use Triangle
or the Search button to enter a title using the Vita keyboard. Search ignores
ASCII case and accepts supported non-ASCII text. Circle returns without losing
the current search and position. Missing or failed covers use placeholders.
A listed game does not guarantee streaming eligibility. The Play handoff is
prepared, but the graphical streaming connection is still the next ticket.
Follow [the catalogue test procedure](CATALOGUE_TEST.md). Report step numbers
and symptoms, never credential files or a full app-data folder.
