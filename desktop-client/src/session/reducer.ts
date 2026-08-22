import type { SessionLifecycleState } from "../ipc/session-schema.ts";

export type SessionAction =
  | Readonly<{ type: "transport-connected" }>
  | Readonly<{ type: "transport-disconnected" }>
  | Readonly<{ type: "request-game-sent" }>
  | Readonly<{ type: "queue-state"; queued: boolean }>
  | Readonly<{ type: "start-game-sent" }>
  | Readonly<{ type: "streaming-state"; streaming: boolean }>
  | Readonly<{ type: "stop-sent" }>
  | Readonly<{ type: "shutdown-state"; shuttingDown: boolean }>
  | Readonly<{ type: "fault" }>;

export interface TransitionResult {
  readonly state: SessionLifecycleState;
  readonly accepted: boolean;
  readonly reason?: string;
}

const reject = (
  state: SessionLifecycleState,
  action: SessionAction,
): TransitionResult => ({
  state,
  accepted: false,
  reason: `Action ${action.type} is not valid from ${state}`,
});

export function reduceSession(
  state: SessionLifecycleState,
  action: SessionAction,
): TransitionResult {
  if (action.type === "fault") {
    return { state: "error", accepted: true };
  }
  if (action.type === "transport-disconnected") {
    return { state: "disconnected", accepted: true };
  }
  if (action.type === "shutdown-state" && action.shuttingDown) {
    return { state: "shutting-down", accepted: true };
  }

  switch (state) {
    case "disconnected":
      return action.type === "transport-connected"
        ? { state: "ready", accepted: true }
        : reject(state, action);

    case "ready":
      return action.type === "request-game-sent"
        ? { state: "allocation-requested", accepted: true }
        : reject(state, action);

    case "allocation-requested":
      if (action.type === "queue-state" && action.queued) {
        return { state: "queued", accepted: true };
      }
      if (action.type === "start-game-sent") {
        return { state: "starting", accepted: true };
      }
      return reject(state, action);

    case "queued":
      if (action.type === "queue-state" && !action.queued) {
        return { state: "allocation-requested", accepted: true };
      }
      if (action.type === "start-game-sent") {
        return { state: "starting", accepted: true };
      }
      return reject(state, action);

    case "starting":
      return action.type === "streaming-state" && action.streaming
        ? { state: "streaming", accepted: true }
        : reject(state, action);

    case "streaming":
      return action.type === "stop-sent"
        ? { state: "stopping", accepted: true }
        : reject(state, action);

    case "stopping":
      return action.type === "streaming-state" && !action.streaming
        ? { state: "ready", accepted: true }
        : reject(state, action);

    case "shutting-down":
      return action.type === "shutdown-state" && !action.shuttingDown
        ? { state: "disconnected", accepted: true }
        : reject(state, action);

    case "error":
      return action.type === "transport-connected"
        ? { state: "ready", accepted: true }
        : reject(state, action);
  }
}

