import type { SessionLifecycleState } from "../ipc/session-schema.ts";
import { SessionController } from "../session/controller.ts";
import { adaptSyntheticSessionEvent } from "../session/synthetic-event-adapter.ts";
import type { SessionAction } from "../session/reducer.ts";
import { executeRecovery } from "../session/recovery-executor.ts";
import { recoveryLimits } from "../session/recovery-policy.ts";
import { SyntheticTransport } from "./synthetic-transport.ts";
import type { DiagnosticSink } from "../diagnostics/recorder.ts";

export type ScenarioName =
  | "successful-session"
  | "queue-then-stream"
  | "shutdown"
  | "network-loss"
  | "transport-failure"
  | "allocation-retry-exhausted"
  | "startup-timeout-recovery";

export interface SimulationStep {
  readonly operation: string;
  readonly state: SessionLifecycleState;
  readonly accepted: boolean;
}

export interface SimulationResult {
  readonly scenario: ScenarioName;
  readonly finalState: SessionLifecycleState;
  readonly commandNames: readonly string[];
  readonly steps: readonly SimulationStep[];
}

const recordTransition = (
  controller: SessionController,
  steps: SimulationStep[],
  operation: string,
  action: SessionAction,
): void => {
  const transition = controller.transition(action);
  steps.push({
    operation,
    state: transition.state,
    accepted: transition.accepted,
  });
};

const recordCommand = async (
  controller: SessionController,
  steps: SimulationStep[],
  operation: "requestGame" | "startGame" | "stop",
): Promise<void> => {
  try {
    const transition = await controller[operation]();
    steps.push({
      operation,
      state: transition.state,
      accepted: transition.accepted,
    });
  } catch {
    steps.push({ operation, state: controller.state, accepted: false });
  }
};

export async function runScenario(
  scenario: ScenarioName,
  diagnostics?: DiagnosticSink,
): Promise<SimulationResult> {
  const transport = new SyntheticTransport();
  const controller = new SessionController(transport, diagnostics);
  const steps: SimulationStep[] = [];

  recordTransition(controller, steps, "connect", { type: "transport-connected" });

  switch (scenario) {
    case "successful-session":
      await recordCommand(controller, steps, "requestGame");
      await recordCommand(controller, steps, "startGame");
      recordTransition(
        controller,
        steps,
        "streaming-on",
        adaptSyntheticSessionEvent({ type: "isStreaming", active: true }),
      );
      await recordCommand(controller, steps, "stop");
      recordTransition(
        controller,
        steps,
        "streaming-off",
        adaptSyntheticSessionEvent({ type: "isStreaming", active: false }),
      );
      break;

    case "queue-then-stream":
      await recordCommand(controller, steps, "requestGame");
      recordTransition(
        controller,
        steps,
        "queued-on",
        adaptSyntheticSessionEvent({ type: "isQueued", active: true }),
      );
      recordTransition(
        controller,
        steps,
        "queued-off",
        adaptSyntheticSessionEvent({ type: "isQueued", active: false }),
      );
      await recordCommand(controller, steps, "startGame");
      recordTransition(
        controller,
        steps,
        "streaming-on",
        adaptSyntheticSessionEvent({ type: "isStreaming", active: true }),
      );
      break;

    case "shutdown":
      recordTransition(
        controller,
        steps,
        "shutdown-on",
        adaptSyntheticSessionEvent({ type: "isShuttingDown", active: true }),
      );
      recordTransition(
        controller,
        steps,
        "shutdown-off",
        adaptSyntheticSessionEvent({ type: "isShuttingDown", active: false }),
      );
      break;

    case "network-loss":
      await recordCommand(controller, steps, "requestGame");
      transport.connected = false;
      recordTransition(controller, steps, "disconnect", {
        type: "transport-disconnected",
      });
      break;

    case "transport-failure":
      transport.failNextSend = true;
      await recordCommand(controller, steps, "requestGame");
      break;

    case "allocation-retry-exhausted":
      await recordCommand(controller, steps, "requestGame");
      for (let attempt = 0; attempt < 3; attempt += 1) {
        const recovery = await executeRecovery(
          controller,
          recoveryLimits.allocationTimeoutMs,
          async () => {},
          diagnostics,
        );
        steps.push({
          operation: `recovery-${recovery.decision.action}`,
          state: controller.state,
          accepted: recovery.transition?.accepted ?? false,
        });
      }
      break;

    case "startup-timeout-recovery": {
      await recordCommand(controller, steps, "requestGame");
      await recordCommand(controller, steps, "startGame");
      const recovery = await executeRecovery(
        controller,
        recoveryLimits.startupTimeoutMs,
        async () => {},
        diagnostics,
      );
      steps.push({
        operation: `recovery-${recovery.decision.action}`,
        state: controller.state,
        accepted: recovery.transition?.accepted ?? false,
      });
      break;
    }
  }

  return {
    scenario,
    finalState: controller.state,
    commandNames: [...transport.commandNames],
    steps,
  };
}

export const scenarioNames: readonly ScenarioName[] = [
  "successful-session",
  "queue-then-stream",
  "shutdown",
  "network-loss",
  "transport-failure",
  "allocation-retry-exhausted",
  "startup-timeout-recovery",
];
