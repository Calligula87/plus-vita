# Recovery and private diagnostics candidate (#19, #20)

Compare this experimental candidate with the accepted
`plus-vita-play-18-interruption-1.vpk` identified in
[STABLE_GRAPHICAL_BASELINE.md](STABLE_GRAPHICAL_BASELINE.md). Keep both VPKs.
The candidate uses `recovery-diagnostics-1` diagnostics and the unchanged accepted
720p input/media profile. It has no app-imposed play cutoff. It is not a public
beta and has not passed #22 or #23.

## Vita checks still required

1. Launch and inspect Settings: log availability must become available after
   storage initializes. Follow the accepted retained-JSON setup normally. Never
   publish JSON, account files, tokens, private screenshots or retrieved logs.
2. Cancel startup authorization/catalogue loading and allocation with Circle.
   Keep browsing while the worker closes; no late completion may start Play.
   Explicitly try again after cleanup. Each actual Play starts a fresh allocation.
3. Compare normal play, picture, sound, mappings/releases and response with the
   stable VPK. Stop with the two-second START+SELECT chord; verify a restored
   catalogue and another successful Play. Numerical input response is unmeasured.
4. During allocation and during play, disconnect Wi-Fi. Verify a readable usable
   screen, released controls and no automatic allocation. Reconnect Wi-Fi and
   explicitly choose Play. Confirm search/selection and sign-in survive.
5. Sleep during connection and play, then wake. Verify the old transport/media
   session is retired and controls/display are usable. Choose Play explicitly
   for a fresh session; no same-session resume is promised. Repeat the sequence.
   Power callback timing and pre-suspend cleanup require actual hardware evidence.
6. With a genuinely rejected/expired test credential, verify renewed JSON setup
   is requested and automatic reuse is blocked. Temporary offline/service/rate
   limit errors must retain sign-in. Do not post credentials or remote bodies.
7. Copy the actual generated log privately with VitaShell before the next Play
   or launch. Verify the public build identity, fixed stages, final stream/UI
   aggregates and numeric loss summary. Return to catalogue must preserve the
   completed file. The next Play/launch resets sequence and replaces it.
8. Check unavailable/full storage where practical: UI and Play remain usable and
   Settings says unavailable. Compare a long session with logging enabled;
   output must remain at most 1,048,576 bytes. The final 45-minute release run
   remains the separate #23 gate.

Record candidate checksum, non-unique device/software configuration, comparison
artifact and pass/fail results. Keep logs local and report sanitized results.
Host tests and a VPK build do not satisfy these hardware checks.
