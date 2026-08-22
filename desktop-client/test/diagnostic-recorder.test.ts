import assert from "node:assert/strict";
import test from "node:test";
import {
  DiagnosticRecorder,
  isSafeDiagnosticEvent,
} from "../src/diagnostics/recorder.ts";
import { SessionController } from "../src/session/controller.ts";
import { executeRecovery } from "../src/session/recovery-executor.ts";
import { recoveryLimits } from "../src/session/recovery-policy.ts";
import { SyntheticTransport } from "../src/simulation/synthetic-transport.ts";

test("recorder accepts allowlisted metadata only", () => {
  const recorder = new DiagnosticRecorder();
  assert.equal(
    recorder.record({
      event: "state-transition",
      from: "ready",
      to: "allocation-requested",
      action: "request-game-sent",
      accepted: true,
    }),
    true,
  );
  assert.equal(
    recorder.record({
      event: "command-outcome",
      command: "requestGame",
      outcome: "sent",
    }),
    true,
  );
  assert.equal(recorder.snapshot().length, 2);
});

test("runtime validation rejects extra, sensitive, endpoint, and arbitrary values", () => {
  assert.equal(
    isSafeDiagnosticEvent({
      event: "command-outcome",
      command: "requestGame",
      outcome: "sent",
      token: "synthetic-secret",
    }),
    false,
  );
  assert.equal(
    isSafeDiagnosticEvent({
      event: "recovery-decision",
      decision: "104.142.181.24:2053",
    }),
    false,
  );
  assert.equal(
    isSafeDiagnosticEvent({
      event: "state-transition",
      from: "ready",
      to: "ready",
      action: "unknown-action",
      accepted: false,
    }),
    false,
  );
});

test("bounded recorder drops oldest entries", () => {
  const recorder = new DiagnosticRecorder(2);
  for (const decision of ["wait", "retry-allocation", "fail"] as const) {
    recorder.record({ event: "recovery-decision", decision });
  }
  assert.deepEqual(
    recorder.snapshot().map((entry) => entry.sequence),
    [2, 3],
  );
});

test("controller and recovery emit safe structured diagnostics", async () => {
  const recorder = new DiagnosticRecorder();
  const controller = new SessionController(new SyntheticTransport(), recorder);
  controller.transition({ type: "transport-connected" });
  await controller.requestGame();
  await executeRecovery(
    controller,
    recoveryLimits.allocationTimeoutMs,
    async () => {},
    recorder,
  );

  const json = recorder.toJSON();
  assert.match(json, /state-transition/);
  assert.match(json, /command-outcome/);
  assert.match(json, /recovery-decision/);
  assert.doesNotMatch(json, /token|session|104\.142|192\.168/iu);
});

