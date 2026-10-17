# Remembered account and typography candidate

This development candidate addresses [#14](https://github.com/Calligula87/plus-vita/issues/14)
and [#16](https://github.com/Calligula87/plus-vita/issues/16), specified in
[#26](https://github.com/Calligula87/plus-vita/issues/26). The approved blue/gold
palette, joystick/touch menus and in-game control mappings are preserved.
It still has no graphical Play action, cover catalogue or title search.

## Install and keep a rollback

Use the uniquely named VPK and SHA256 in the handoff. Installing replaces
the existing app (shared title ID). Preserve the accepted stream VPK described
in [the milestone](PLAYABLE_MILESTONE.md) for rollback. The new font rendering
and remembered-account behavior need this Vita check; host success is not
hardware acceptance.

## What to test

1. Open the app, then About. Compare normal viewing-size text with the previous
   candidate: headings, body paragraphs, button labels and hints should be clear,
   with complete letter tops/bottoms and no accidental cut text or overlap.
   Long single-line labels end deliberately with `...`; prose wraps to words.
   Check that the approved colours and general sans-serif style remain suitable.
2. Check Get started and the full setup guide. Use the joystick, Cross/Circle
   and touch, including held input/repeat. D-pad presses must not move menu
   focus. No diagnostic terminal should appear.
3. Transfer a fresh JSON using [the setup guide](ACCOUNT_SETUP.md). In setup,
   **Remember this account defaults Off**. Select it once to turn it **On**, then
   select **Import JSON**. The import is removed; sign-in must reach catalogue.
4. Press Circle and verify **Open catalogue** returns to the previous selection.
   Open **Account** from home: Remember should be On. This means the minimum
   credential is saved in a locally readable file on the Vita, not encrypted
   storage or a Sony password vault.
5. **Actually close the application and reopen it.** With no transfer JSON
   present, it should show restoration progress, validate the saved sign-in
   and open the catalogue. Repeat once more. No fresh transfer should be needed
   while Sony accepts the credential. This does not promise a fixed lifetime,
   automatic token renewal or continuation of the same cloud gaming session.
6. In Account, use **Edit account label**. The optional label is entered by you,
   not a Sony-verified email/ID. Test an accented/non-ASCII name, cancel the
   keyboard once, then save a label. Rendering must return cleanly with no replay
   of Enter/Cancel; reopen the app and verify the remembered label remains.
   The keyboard accepts up to 32 UTF-16 code units; the label is bounded to
   32 Unicode scalars / 128 UTF-8 bytes. Unsupported font glyphs use a placeholder.
7. Turn Remember Off in Account. The current runtime account/catalogue remains
   usable. Close/reopen: it must no longer restore that account. Re-import or
   enable Remember again before exiting if you want it retained.
8. With a remembered account, reopen while Wi-Fi is unavailable. It should show
   a fixed failure with Retry, retaining the saved sign-in. Restore Wi-Fi and
   retry without transferring another JSON. Rate limits/service failures likewise
   must not falsely declare expiry. Confirmed authorization rejection (currently
   HTTP 401 or explicit OAuth login-required/invalid-grant redirect) removes/blocks the invalid saved account
   and asks for fresh JSON. Other unexpected service responses remain retryable;
   you can deliberately Forget and import fresh JSON if necessary.
9. Choose **Forget account**, cancel confirmation once, then confirm it. This
   clears runtime credentials, label/catalogue and the saved account, removes any
   stale managed import and returns to setup. Reopen: no account should restore.
10. If a storage error appears, record which action failed. Save failure must
    not claim the account changes were saved; runtime use remains possible.
    Delete failure must say removal is incomplete and offer recovery. A local
    block marker prevents a failed deletion from silently restoring the old file.

## Privacy and evidence

Report step numbers, pass/fail/not-tested and visible symptoms. Never attach
the imported JSON, remembered account file, label-containing private screenshots,
browser response or a whole app-data folder. Saved data lives below
`ux0:data/plus-vita/`; it is runtime private data, never a VPK resource.

The current font adapter rasterizes installed system outline fonts at the
requested size instead of enlarging a small PGF bitmap. It places glyphs using
measured bearings/advances and uses bounded text regions with explicit overflow.
If system outline fonts are unavailable, the SDK bitmap fallback remains.
The API initialization/bearing conventions were checked against the public
[libvita2d PVF adapter](https://github.com/xerpi/libvita2d/blob/master/libvita2d/source/vita2d_pvf.c).
No Sony font asset is distributed. Physical sharpness, exact font appearance
and keyboard/display handoff remain unverified until this procedure is run.

The final beta's 45-minute play, sleep/recovery and reporting/privacy gates
remain separate in [#23](https://github.com/Calligula87/plus-vita/issues/23).
