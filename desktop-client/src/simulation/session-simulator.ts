import type { SessionLifecycleState } from "../ipc/session-schema.ts";
import { SessionController } from "../session/controller.ts";
import { adaptSyntheticSessionEvent } from "../session/synthetic-event-adapter.ts";
import type { SessionAction } from "../session/reducer.ts";
import { SyntheticTransport } from "./synthetic-transport.ts";

export type ScenarioName =
  | "successful-session"
  | "queue-then-stream"
  | "shutdown"
  | "network-loss"
  | "transport-failure";

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

export async function runScenario(scenario: ScenarioName): Promise<SimulationResult> {
  const transport = new SyntheticTransport();
  const controller = new SessionController(transport);
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
];

