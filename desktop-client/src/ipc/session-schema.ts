/**
 * Value-free types for the statically documented AGL-to-QAS session boundary.
 *
 * These declarations intentionally contain no authentication, account,
 * entitlement, game, client, or session values. Unknown payloads stay opaque
 * until their field names can be established without collecting secrets.
 */

export type EmptyParams = Readonly<Record<string, never>>;

export interface QasCommandEnvelope<
  Command extends string,
  Params extends Readonly<object>,
> {
  readonly command: Command;
  readonly params: Params;
  readonly source: string;
  readonly target: "QAS";
}

export type RequestClientIdCommand = QasCommandEnvelope<
  "requestClientId",
  EmptyParams
>;

export type RequestGameCommand = QasCommandEnvelope<
  "requestGame",
  Readonly<{ forceLogout: boolean }>
>;

export type StartGameCommand = QasCommandEnvelope<"startGame", EmptyParams>;

/** The installed bridge treats the switch-game specification as opaque. */
export type RequestSwitchGameCommand = QasCommandEnvelope<
  "requestSwitchGame",
  Readonly<{ switchGameSpec: unknown }>
>;

export type StopCommand = QasCommandEnvelope<"stop", EmptyParams>;

export type IsStreamingCommand = QasCommandEnvelope<
  "isStreaming",
  EmptyParams
>;

export type IsQueuedCommand = QasCommandEnvelope<"isQueued", EmptyParams>;

export type IsShuttingDownCommand = QasCommandEnvelope<
  "isShuttingDown",
  EmptyParams
>;

export type SessionCommand =
  | RequestClientIdCommand
  | RequestGameCommand
  | StartGameCommand
  | RequestSwitchGameCommand
  | StopCommand
  | IsStreamingCommand
  | IsQueuedCommand
  | IsShuttingDownCommand;

export type SessionStateEventType =
  | "isStreaming"
  | "isQueued"
  | "isShuttingDown";

/**
 * The bridge confirms these event discriminators, but not their value-field
 * names. Consumers must not guess at or persist unknown values.
 */
export type SessionStateEvent = Readonly<
  { type: SessionStateEventType } & Record<string, unknown>
>;

export type SessionLifecycleState =
  | "disconnected"
  | "ready"
  | "allocation-requested"
  | "queued"
  | "starting"
  | "streaming"
  | "stopping"
  | "shutting-down"
  | "error";
