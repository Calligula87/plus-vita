import { DiagnosticRecorder } from "../diagnostics/recorder.ts";
import {
  runScenario,
  scenarioNames,
  type SimulationResult,
} from "../simulation/session-simulator.ts";

export interface ScenarioDemonstration {
  readonly result: SimulationResult;
  readonly diagnostics: ReturnType<DiagnosticRecorder["snapshot"]>;
}

export interface FeasibilityDemonstration {
  readonly mode: "offline-synthetic";
  readonly externalConnections: false;
  readonly scenarios: readonly ScenarioDemonstration[];
}

export async function runFeasibilityDemo(): Promise<FeasibilityDemonstration> {
  const scenarios: ScenarioDemonstration[] = [];

  for (const scenarioName of scenarioNames) {
    const diagnostics = new DiagnosticRecorder();
    const result = await runScenario(scenarioName, diagnostics);
    scenarios.push({ result, diagnostics: diagnostics.snapshot() });
  }

  return {
    mode: "offline-synthetic",
    externalConnections: false,
    scenarios,
  };
}

