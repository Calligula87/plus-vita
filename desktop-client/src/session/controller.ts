import type { SessionCommand } from "../ipc/session-schema.ts";
import type { SessionTransport } from "../ipc/transport.ts";
import {
  reduceSession,
  type SessionAction,
  type TransitionResult,
} from "./reducer.ts";
import type { SessionLifecycleState } from "../ipc/session-schema.ts";

export class SessionController {
  readonly #transport: SessionTransport;
  #state: SessionLifecycleState = "disconnected";

  constructor(transport: SessionTransport) {
    this.#transport = transport;
  }

  get state(): SessionLifecycleState {
    return this.#state;
  }

  transition(action: SessionAction): TransitionResult {
    const result = reduceSession(this.#state, action);
    if (result.accepted) {
      this.#state = result.state;
    }
    return result;
  }

  async requestGame(forceLogout = false): Promise<TransitionResult> {
    const transition = this.transition({ type: "request-game-sent" });
    if (!transition.accepted) return transition;

    await this.#send({
      command: "requestGame",
      params: { forceLogout },
      source: "reference-client",
      target: "QAS",
    });
    return transition;
  }

  async startGame(): Promise<TransitionResult> {
    const transition = this.transition({ type: "start-game-sent" });
    if (!transition.accepted) return transition;

    await this.#send({
      command: "startGame",
      params: {},
      source: "reference-client",
      target: "QAS",
    });
    return transition;
  }

  async stop(): Promise<TransitionResult> {
    const transition = this.transition({ type: "stop-sent" });
    if (!transition.accepted) return transition;

    await this.#send({
      command: "stop",
      params: {},
      source: "reference-client",
      target: "QAS",
    });
    return transition;
  }

  async #send(command: SessionCommand): Promise<void> {
    if (!this.#transport.connected) {
      this.transition({ type: "transport-disconnected" });
      throw new Error("Session transport is disconnected");
    }
    try {
      await this.#transport.send(command);
    } catch (cause) {
      this.transition({ type: "fault" });
      throw new Error("Session transport send failed", { cause });
    }
  }
}
