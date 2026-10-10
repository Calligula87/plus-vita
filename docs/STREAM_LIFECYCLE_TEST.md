# Stream stop and new-session test

Build: `live-720p-refs5-input-5-clean-reconnect`.

1. Install `build/vita/plus_vita_smoke.vpk` and start a PS4 cloud title normally.
2. After video, audio, and input work, hold **START + SELECT together for two seconds**. A short press must not end the stream. While both buttons are held, neither Options nor Share is forwarded to the game.
3. Expect the allocation-result screen with **Transport: ENDED BY USER**. The client sends an idle controller state, a Takion disconnect, closes transport, and releases decoder, display, audio, and touch resources.
4. Press **X** on that screen to request a **new cloud allocation** for the same title without restarting the app. This is a new session, not an automatic recovery of the prior allocation or a guarantee of retained in-game state.
5. Confirm video, audio, and input again. Stop once more, then press **CIRCLE** to return to title details.

The bounded ten-minute preview limit remains. Unexpected network loss still reports a failure; automatic reconnection after loss is not part of this increment. The allocation diagnostic remains at `ux0:data/plus-vita-allocation.log` and is replaced when a new allocation begins. The credential file is not changed by this test.
