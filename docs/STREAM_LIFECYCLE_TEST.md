# Stream stop and new-session test

Current accepted build: `live-720p-refs5-video-reorder-1` (2026-10-13).
Audio recording is off. Rear upper-left/right maps to L2/R2; lower-left/right
maps to L3/R3. Video FEC and the bounded late-packet window are on; the failed
display-worker experiment is reverted. See `VIDEO_FEC_TEST.md` for hardware
results and `DISPLAY_PACING_TEST.md` for the rollback record.

1. Install `build/vita/plus_vita_smoke.vpk` and start a PS4 cloud title normally.
2. After video, audio, and input work, hold **START + SELECT together for two seconds**. A short press must not end the stream. While both buttons are held, neither Options nor Share is forwarded to the game.
3. Expect the allocation-result screen with **Transport: ENDED BY USER**. The client sends an idle controller state, a Takion disconnect, closes transport, and releases decoder, display, audio, and touch resources.
4. Press **X** on that screen to request a **new cloud allocation** for the same title without restarting the app. This is a new session, not an automatic recovery of the prior allocation or a guarantee of retained in-game state.
5. Confirm video, audio, and input again. Stop once more, then press **CIRCLE** to return to title details.

The bounded ten-minute preview limit remains. Unexpected network loss still reports a failure; automatic reconnection after loss is not part of this increment. The allocation diagnostic remains at `ux0:data/plus-vita-allocation.log` and is replaced when a new allocation begins. The credential file is not changed by this test.

## Recovery timer regression check

The input-6 hardware log ended after about 7.6 minutes: a recovery IDR arrived after 179 ms, followed immediately by `preview frame deadline expired`. Input/congestion sends release the mutex; recovery can update its timestamp while the loop retains an older clock reading. Unsigned subtraction then falsely expires the output deadline.

Input-7 samples time under the mutex immediately before deadline checks and guards recovery timestamp subtraction. Timeout messages include the measured output idle time and pending frame count.

Play for eight to nine minutes, using rear upper-left/right for L2/R2 and lower-left/right for L3/R3. After a brief video freeze, expect `recovery resumed` followed by `recovery output displayed`, with the session continuing. Stop with START + SELECT before the ten-minute limit and reconnect once. Retrieve the log to check recovery and clean shutdown. The historical input-7 timer fix did not add parity reconstruction; the current reorder build additionally enables FEC and a short late-packet window.

## Input-7 hardware result

`build/debug/plus-vita-input-7-result-20261010.log` completed the ten-minute preview with `result=0`, `err=0`, and a clean Takion disconnect. All 45 video recovery episodes resumed and displayed output, including recovery at 592 seconds; no false frame timeout occurred. Audio reported zero playback failures. This run ended at the preview limit, so it does not additionally validate the user-stop/reconnect sequence. Input-8 removes automatic audio recording after the user accepted audio quality.
