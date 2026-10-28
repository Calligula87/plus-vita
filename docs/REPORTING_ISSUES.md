# Report a Plus Vita issue

Use [GitHub Issues](https://github.com/Calligula87/plus-vita/issues/new/choose) to report a reproducible problem. Include the app version shown under **Settings and help**, what you did, what you expected, and what happened. A non-unique Vita model/software version and whether Wi-Fi was connected can help. Do not post a device serial number, network address or account name.

**Priority policy:** include a reviewed current-session log for priority investigation. Reports without a log are accepted but are not treated as priority. If the log is unavailable, say so and provide reproduction steps and symptoms.

## Copy the current session log

Plus Vita starts a fresh log each time the graphical app launches and before each explicit Play attempt at `ux0:data/plus-vita-allocation.log`. It records app stages and aggregate timing; it is not a recording of your screen or account. Using VitaShell, copy the log to your PC before another Play attempt or app relaunch replaces it. Review the copy yourself, then attach only that log to your GitHub issue for priority investigation. The app never uploads it or opens an issue automatically.

**Never attach** `ux0:data/plus-vita-auth.json`, `ux0:data/plus-vita/account.json`, an entire `ux0:data` folder, Sony browser responses, passwords, tokens, authenticated network captures or private screenshots. The sign-in JSON remains locally readable and must stay private. If Settings says the log is unavailable, describe the steps and result without one.

For catalogue or setup problems, mention the screen and action. For streaming problems, mention whether the issue occurred while connecting, during play, after START+SELECT, after Wi-Fi loss, or after sleep/wake. A listed title or missing cover is not proof of streaming eligibility.

The [v000 version](V000.md) imports a Sony sign-in JSON made on a phone or PC, supports one account and uses an English interface. It does not offer direct Sony sign-in on the Vita or automatic credential renewal. Graphical Play uses the accepted 720p stream profile and has no app-imposed ten-minute cutoff. The maintainer confirms the 45-minute v000 session, stop/relaunch, Wi-Fi and sleep recovery, Remember/Forget/restart and search/back-navigation checks passed. Software privacy checks also passed. The identified VPK and matching source are available from the [v000 release](https://github.com/Calligula87/plus-vita/releases/tag/v000). A lost cloud session requires an explicit fresh attempt; same-session resume is not promised. The older [stable graphical baseline](STABLE_GRAPHICAL_BASELINE.md) remains preserved for comparison.
