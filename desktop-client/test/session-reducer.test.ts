import assert from "node:assert/strict";
import test from "node:test";
import type { SessionLifecycleState } from "../src/ipc/session-schema.ts";
import { reduceSession, type SessionAction } from "../src/session/reducer.ts";

function step(
  state: SessionLifecycleState,
  action: SessionAction,
): SessionLifecycleState {
  const result = reduceSession(state, action);
  assert.equal(result.accepted, true, result.reason);
  return result.state;
}

test("synthetic happy path reaches streaming and returns to ready", () => {
  let state: SessionLifecycleState = "disconnected";
  state = step(state, { type: "transport-connected" });
  state = step(state, { type: "request-game-sent" });
  state = step(state, { type: "queue-state", queued: true });
  state = step(state, { type: "queue-state", queued: false });
  state = step(state, { type: "start-game-sent" });
  state = step(state, { type: "streaming-state", streaming: true });
  state = step(state, { type: "stop-sent" });
  state = step(state, { type: "streaming-state", streaming: false });
  assert.equal(state, "ready");
});

test("invalid synthetic transition preserves state", () => {
  const result = reduceSession("ready", { type: "start-game-sent" });
  assert.equal(result.accepted, false);
  assert.equal(result.state, "ready");
  assert.match(result.reason ?? "", /not valid/);
});

test("disconnect and fault are accepted from active states", () => {
  assert.deepEqual(reduceSession("streaming", { type: "transport-disconnected" }), {
    state: "disconnected",
    accepted: true,
  });
  assert.deepEqual(reduceSession("queued", { type: "fault" }), {
    state: "error",
    accepted: true,
  });
});

