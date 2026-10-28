<p align="center">
  <img src="vita-client/assets/branding/logo-source.png" alt="Plus-Vita logo" width="200">
</p>

<h1 align="center">Plus-Vita</h1>

## Introduction

Plus-Vita lets you play eligible **PlayStation Plus cloud games directly on your
PlayStation Vita**. You need your own Sony account and
**a PlayStation Plus subscription tier that includes cloud streaming in your
region**. 

[**Download Plus-Vita v000**](https://github.com/Calligula87/plus-vita/releases/tag/v000)

This is an unofficial homebrew project, not affiliated with Sony Interactive
Entertainment. No PC relay is needed during play; initial sign-in uses a phone
or PC browser.

## Requirements

- A homebrew-enabled PS Vita with **VitaShell** and working Wi-Fi.
- Your own Sony account with a subscription that supports PlayStation Plus cloud
  streaming in your region.
- A phone or PC browser to create your sign-in JSON, plus USB or FTP file transfer.
- Your private **plus-vita-auth.json** file at **ux0:data/plus-vita-auth.json**,
  created using the setup below.

## Setup

1. Download **plus-vita-v000.vpk**, copy it to your Vita and install it using
   VitaShell. Existing Plus-Vita installations are updated using the same app ID.
2. On your phone or PC, sign in to your own account at
   [PlayStation](https://www.playstation.com/). Complete Sony's verification or
   passkey prompts there.
3. In the **same browser and profile**, open the
   [Sony sign-in response](https://ca.account.sony.com/api/v1/ssocookie).
   It should display JSON containing an `npsso` field. If it does not, sign in
   again and retry.
4. Save that response itself as **plus-vita-auth.json**, using the browser's
   save/download action, usually **Ctrl+S** on a PC. Save JSON, not an HTML page,
   screenshot or a file ending in `.json.txt`. You do not need to extract or type
   the token.
5. Transfer the file through VitaShell to **ux0:data/plus-vita-auth.json**.
6. Launch **Plus-Vita**. The app validates your saved sign-in and opens the
   catalogue. If the app is already open, choose **Get started → Continue →
   Import JSON**. Select a game, open its details and choose **Play**.

**Keep the JSON private: it grants account access. Never upload it to GitHub or
share its contents.** It remains on your Vita. Remember defaults On; Remember Off
and Forget pause automatic reuse until you explicitly reconnect. Account files
are readable by someone with Vita file access. If Sony rejects an expired
credential, save a fresh response and explicitly import it again.

See the [full setup and account-recovery guide](docs/ACCOUNT_SETUP.md) for help.
After Wi-Fi loss or sleep, Retry starts a fresh cloud connection; it does not
resume the previous session.

## Report an issue

If you encounter a problem, [submit a GitHub ticket](https://github.com/Calligula87/plus-vita/issues/new/choose).
Include **v000**, the steps to reproduce it, what you expected and what happened.

**A session log is needed for priority investigation. Reports without a log will
not be treated as priority.** To provide one:

1. Reproduce the issue, then use VitaShell to copy
   **ux0:data/plus-vita-allocation.log** to your phone or PC.
2. Copy it **before another app launch or Play attempt**, which replaces the log.
3. Review it privately and attach **only the reviewed log** to your ticket.

Never attach `plus-vita-auth.json`, saved account files, tokens, Sony browser
responses, private screenshots or an entire app-data folder. The app does not
upload logs automatically. If no log is available, explain that in the ticket
and describe the symptoms; the report can still be submitted without priority.
See the [reporting guide](docs/REPORTING_ISSUES.md).

## Controls

| Action | Control |
| --- | --- |
| Move through menus | Left stick or touch |
| Select / go back | Cross / Circle |
| Search catalogue | Triangle or Search button |
| In-game buttons and sticks | Corresponding PlayStation controls |
| L2 / R2 | Rear touchpad top-left / top-right |
| L3 / R3 | Rear touchpad bottom-left / bottom-right |
| Move / press the controller touchpad | Front touch / two front touches |
| Stop streaming and return | Hold **START + SELECT for two seconds** |
| Skip opening splash | Cross or touch |

The D-pad works in games; it does not navigate app menus. **Settings and help**
contains the controller guide, account controls, app version and log status.
The interface is English, supports one account and uses a fixed 720p stream
profile. Stream-quality tuning and controller remapping are not included.

---

Built on [Pylux/libchiaki](https://github.com/ForWard-Technologies-LLC/Pylux) and
[VitaSDK](https://vitasdk.org/). See [source/build instructions](release/README.md),
[licenses and credits](release/THIRD_PARTY_NOTICES.md), and the
[v000 identity and validation record](docs/V000.md). Source is provided under the
[AGPL-3.0](LICENSE); third-party components retain their own license terms.
