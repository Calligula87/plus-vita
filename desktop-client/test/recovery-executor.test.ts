import assert from "node:assert/strict";
import test from "node:test";
import { SessionController } from "../src/session/controller.ts";
import { executeRecovery } from "../src/session/recovery-executor.ts";
import { recoveryLimits } from "../src/session/recovery-policy.ts";
import { SyntheticTransport } from "../src/simulation/synthetic-transport.ts";

const readyController = (): {
  controller: SessionController;
  transport: SyntheticTransport;
} => {
  const transport = new SyntheticTransport();
  const controller = new SessionController(transport);
  controller.transition({ type: "transport-connected" });
  return { controller, transport };
};

test("allocation recovery executes bounded retries with injected delays", async () => {
  const { controller, transport } = readyController();
  const delays: number[] = [];
  await controller.requestGame();

  const first = await executeRecovery(
    controller,
    recoveryLimits.allocationTimeoutMs,
    async (delayMs) => void delays.push(delayMs),
  );
  const second = await executeRecovery(
    controller,
    recoveryLimits.allocationTimeoutMs,
    async (delayMs) => void delays.push(delayMs),
  );
  const exhausted = await executeRecovery(
    controller,
    recoveryLimits.allocationTimeoutMs,
    async (delayMs) => void delays.push(delayMs),
  );

  assert.equal(first.transition?.accepted, true);
  assert.equal(second.transition?.accepted, true);
  assert.deepEqual(delays, [1_000, 2_000]);
  assert.equal(controller.allocationAttempts, 3);
  assert.equal(exhausted.decision.action, "fail");
  assert.equal(controller.state, "error");
  assert.deepEqual(transport.commandNames, [
    "requestGame",
    "requestGame",
    "requestGame",
  ]);
});

test("startup recovery sends stop and enters error", async () => {
  const { controller, transport } = readyController();
  await controller.requestGame();
  await controller.startGame();

  const result = await executeRecovery(
    controller,
    recoveryLimits.startupTimeoutMs,
    async () => {},
  );

  assert.equal(result.decision.action, "stop-and-fail");
  assert.equal(controller.state, "error");
  assert.deepEqual(transport.commandNames, ["requestGame", "startGame", "stop"]);
});

test("queue timeout does not mutate state or send", async () => {
  const { controller, transport } = readyController();
  await controller.requestGame();
  controller.transition({ type: "queue-state", queued: true });

  const result = await executeRecovery(
    controller,
    recoveryLimits.queueDecisionTimeoutMs,
    async () => {},
  );

  assert.equal(result.decision.action, "require-user-decision");
  assert.equal(result.transition, undefined);
  assert.equal(controller.state, "queued");
  assert.deepEqual(transport.commandNames, ["requestGame"]);
});

test("stopping timeout disconnects locally", async () => {
  const { controller } = readyController();
  await controller.requestGame();
  await controller.startGame();
  controller.transition({ type: "streaming-state", streaming: true });
  await controller.stop();

  const result = await executeRecovery(
    controller,
    recoveryLimits.stopTimeoutMs,
    async () => {},
  );

  assert.equal(result.decision.action, "disconnect");
  assert.equal(controller.state, "disconnected");
});

