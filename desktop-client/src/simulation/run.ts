import { runScenario, scenarioNames } from "./session-simulator.ts";

const results = await Promise.all(scenarioNames.map(runScenario));
process.stdout.write(`${JSON.stringify(results, null, 2)}\n`);

