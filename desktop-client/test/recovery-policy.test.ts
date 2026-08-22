import assert from "node:assert/strict";
import test from "node:test";
import { evaluateRecovery, recoveryLimits } from "../src/session/recovery-policy.ts";

test("allocation retries are bounded and deterministic", () => {
  assert.deepEqual(
    evaluateRecovery({
      state: "allocation-requested",
      elapsedMs: recoveryLimits.allocationTimeoutMs,
      allocationAttempts: 1,
    }),
    { action: "retry-allocation", delayMs: 1_000 },
  );
  assert.deepEqual(
    evaluateRecovery({
      state: "allocation-requested",
      elapsedMs: recoveryLimits.allocationTimeoutMs,
      allocationAttempts: 2,
    }),
    { action: "retry-allocation", delayMs: 2_000 },
  );
  assert.deepEqual(
    evaluateRecovery({
      state: "allocation-requested",
      elapsedMs: recoveryLimits.allocationTimeoutMs,
      allocationAttempts: 3,
    }),
    { action: "fail", reason: "allocation-attempts-exhausted" },
  );
});

test("queue timeout requires user choice", () => {
  assert.deepEqual(
    evaluateRecovery({
      state: "queued",
      elapsedMs: recoveryLimits.queueDecisionTimeoutMs,
      allocationAttempts: 1,
    }),
    { action: "require-user-decision", reason: "queue-timeout" },
  );
});

test("startup timeout stops and fails", () => {
  assert.deepEqual(
    evaluateRecovery({
      state: "starting",
      elapsedMs: recoveryLimits.startupTimeoutMs,
      allocationAttempts: 1,
    }),
    { action: "stop-and-fail", reason: "startup-timeout" },
  );
});

test("stop and shutdown timeouts disconnect", () => {
  assert.deepEqual(
    evaluateRecovery({
      state: "stopping",
      elapsedMs: recoveryLimits.stopTimeoutMs,
      allocationAttempts: 1,
    }),
    { action: "disconnect", reason: "stop-timeout" },
  );
  assert.deepEqual(
    evaluateRecovery({
      state: "shutting-down",
      elapsedMs: recoveryLimits.shutdownTimeoutMs,
      allocationAttempts: 1,
    }),
    { action: "disconnect", reason: "shutdown-timeout" },
  );
});

test("negative elapsed time is treated as zero", () => {
  assert.deepEqual(
    evaluateRecovery({
      state: "allocation-requested",
      elapsedMs: -1,
      allocationAttempts: 1,
    }),
    { action: "wait" },
  );
});

