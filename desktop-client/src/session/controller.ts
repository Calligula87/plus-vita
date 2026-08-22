import type { SessionCommand } from "../ipc/session-schema.ts";
import type { SessionTransport } from "../ipc/transport.ts";
import {
  reduceSession,
  type SessionAction,
  type TransitionResult,
} from "./reducer.ts";
import type { SessionLifecycleState } from "../ipc/session-schema.ts";
import {
  nullDiagnosticSink,
  type DiagnosticSink,
} from "../diagnostics/recorder.ts";

export class SessionController {
  readonly #transport: SessionTransport;
  readonly #diagnostics: DiagnosticSink;
  #state: SessionLifecycleState = "disconnected";
  #allocationAttempts = 0;

  constructor(
    transport: SessionTransport,
    diagnostics: DiagnosticSink = nullDiagnosticSink,
  ) {
    this.#transport = transport;
    this.#diagnostics = diagnostics;
  }

  get state(): SessionLifecycleState {
    return this.#state;
  }

  get allocationAttempts(): number {
    return this.#allocationAttempts;
  }

  transition(action: SessionAction): TransitionResult {
    const previousState = this.#state;
    const result = reduceSession(this.#state, action);
    this.#diagnostics.record({
      event: "state-transition",
      from: previousState,
      to: result.state,
      action: action.type,
      accepted: result.accepted,
    });
    if (result.accepted) {
      this.#state = result.state;
      if (result.state === "ready" && previousState !== "ready") {
        this.#allocationAttempts = 0;
      }
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
    this.#allocationAttempts = 1;
    return transition;
  }

  async retryRequestGame(forceLogout = false): Promise<TransitionResult> {
    const transition = this.transition({ type: "allocation-retry-sent" });
    if (!transition.accepted) return transition;

    await this.#send({
      command: "requestGame",
      params: { forceLogout },
      source: "reference-client",
      target: "QAS",
    });
    this.#allocationAttempts += 1;
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

  async abortStartup(): Promise<TransitionResult> {
    if (this.#state !== "starting") {
      return {
        state: this.#state,
        accepted: false,
        reason: `Startup abort is not valid from ${this.#state}`,
      };
    }

    await this.#send({
      command: "stop",
      params: {},
      source: "reference-client",
      target: "QAS",
    });
    return this.transition({ type: "startup-timeout" });
  }

  async #send(command: SessionCommand): Promise<void> {
    if (!this.#transport.connected) {
      this.transition({ type: "transport-disconnected" });
      this.#diagnostics.record({
        event: "command-outcome",
        command: command.command,
        outcome: "rejected",
      });
      throw new Error("Session transport is disconnected");
    }
    try {
      await this.#transport.send(command);
      this.#diagnostics.record({
        event: "command-outcome",
        command: command.command,
        outcome: "sent",
      });
    } catch (cause) {
      this.#diagnostics.record({
        event: "command-outcome",
        command: command.command,
        outcome: "failed",
      });
      this.transition({ type: "fault" });
      throw new Error("Session transport send failed", { cause });
    }
  }
}
