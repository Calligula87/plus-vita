import assert from "node:assert/strict";
import test from "node:test";
import { runFeasibilityDemo } from "../src/demo/feasibility-demo.ts";
import { scenarioNames } from "../src/simulation/session-simulator.ts";

test("offline feasibility demo runs every scenario", async () => {
  const demo = await runFeasibilityDemo();
  assert.equal(demo.mode, "offline-synthetic");
  assert.equal(demo.externalConnections, false);
  assert.deepEqual(
    demo.scenarios.map((scenario) => scenario.result.scenario),
    scenarioNames,
  );
});

test("demo output contains only safe diagnostic metadata", async () => {
  const json = JSON.stringify(await runFeasibilityDemo());

  for (const forbidden of [
    /auth/iu,
    /token/iu,
    /cookie/iu,
    /sessionId/iu,
    /sessionKey/iu,
    /params/iu,
    /payload/iu,
    /timestamp/iu,
    /\b(?:\d{1,3}\.){3}\d{1,3}\b/u,
    /"[^"]*:\d{2,5}"/u,
  ]) {
    assert.doesNotMatch(json, forbidden);
  }
});

test("demo is deterministic", async () => {
  assert.deepEqual(await runFeasibilityDemo(), await runFeasibilityDemo());
});
