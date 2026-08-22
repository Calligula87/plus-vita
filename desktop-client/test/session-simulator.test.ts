import assert from "node:assert/strict";
import test from "node:test";
import { runScenario } from "../src/simulation/session-simulator.ts";

test("successful session returns to ready", async () => {
  const result = await runScenario("successful-session");
  assert.equal(result.finalState, "ready");
  assert.deepEqual(result.commandNames, ["requestGame", "startGame", "stop"]);
  assert.equal(result.steps.every((step) => step.accepted), true);
});

test("queue scenario reaches streaming", async () => {
  const result = await runScenario("queue-then-stream");
  assert.equal(result.finalState, "streaming");
  assert.deepEqual(result.commandNames, ["requestGame", "startGame"]);
});

test("shutdown scenario disconnects", async () => {
  const result = await runScenario("shutdown");
  assert.equal(result.finalState, "disconnected");
});

test("network loss disconnects an active allocation", async () => {
  const result = await runScenario("network-loss");
  assert.equal(result.finalState, "disconnected");
  assert.deepEqual(result.commandNames, ["requestGame"]);
});

test("transport failure enters error without recording a command", async () => {
  const result = await runScenario("transport-failure");
  assert.equal(result.finalState, "error");
  assert.deepEqual(result.commandNames, []);
  assert.equal(result.steps.at(-1)?.accepted, false);
});

