import type { SessionCommand, SessionStateEventType } from "./session-schema.ts";

const sessionEventTypes = new Set<SessionStateEventType>([
  "isQueued",
  "isStreaming",
  "isShuttingDown",
]);

const sensitiveKeys = new Set([
  "authCode",
  "authToken",
  "cloudAuthCode",
  "encryptedKey",
  "ipcToken",
  "sessionId",
  "sessionKey",
  "token",
]);

const isRecord = (value: unknown): value is Record<string, unknown> =>
  typeof value === "object" && value !== null && !Array.isArray(value);

const hasOnlyKeys = (record: Record<string, unknown>, allowed: string[]): boolean => {
  const allowedSet = new Set(allowed);
  return Object.keys(record).every((key) => allowedSet.has(key));
};

export interface AcceptedEventShape {
  readonly accepted: true;
  readonly type: SessionStateEventType;
  readonly fieldNames: readonly string[];
}

export interface RejectedEventShape {
  readonly accepted: false;
  readonly reason:
    | "not-an-object"
    | "unsupported-discriminator"
    | "sensitive-field-name";
}

export type EventShapeInspection = AcceptedEventShape | RejectedEventShape;

/**
 * Inspects names only. The result deliberately contains no input values.
 */
export function inspectSessionEventShape(value: unknown): EventShapeInspection {
  if (!isRecord(value)) {
    return { accepted: false, reason: "not-an-object" };
  }

  const type = value.type;
  if (typeof type !== "string" || !sessionEventTypes.has(type as SessionStateEventType)) {
    return { accepted: false, reason: "unsupported-discriminator" };
  }

  const fieldNames = Object.keys(value).sort();
  if (fieldNames.some((fieldName) => sensitiveKeys.has(fieldName))) {
    return { accepted: false, reason: "sensitive-field-name" };
  }

  return {
    accepted: true,
    type: type as SessionStateEventType,
    fieldNames,
  };
}

/** Validates only command shapes confirmed by the installed static bridge. */
export function isConfirmedSessionCommand(value: unknown): value is SessionCommand {
  if (!isRecord(value) || !hasOnlyKeys(value, ["command", "params", "source", "target"])) {
    return false;
  }
  if (value.target !== "QAS" || typeof value.source !== "string" || !value.source) {
    return false;
  }
  if (!isRecord(value.params)) {
    return false;
  }
  if (Object.keys(value.params).some((key) => sensitiveKeys.has(key))) {
    return false;
  }

  switch (value.command) {
    case "requestGame":
      return (
        hasOnlyKeys(value.params, ["forceLogout"]) &&
        typeof value.params.forceLogout === "boolean"
      );

    case "requestClientId":
    case "startGame":
    case "stop":
    case "isStreaming":
    case "isQueued":
    case "isShuttingDown":
      return Object.keys(value.params).length === 0;

    // Opaque switchGameSpec cannot yet be validated safely.
    case "requestSwitchGame":
    default:
      return false;
  }
}

