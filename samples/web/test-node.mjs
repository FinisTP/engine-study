// Runs the WebAssembly build of the turn rules in Node and checks it against the native
// regression (tests/turncombat/TurnEncounterTests.cpp): 26 actions, round 12, hero at 32 health.
import { readFileSync } from 'node:fs';
import { loadSimulation } from './umbra_sim.js';

const here = new URL('.', import.meta.url);
const sim = await loadSimulation(readFileSync(new URL('umbra_sim.wasm', here)));
const warnings = sim.load(readFileSync(new URL('../../assets/rpg/reference.rpg.json', here), 'utf8'));
if (!sim.reset()) throw new Error('reset failed');
while (sim.stepAi()) {}
const s = sim.state();
const hero = s.fighters.find((f) => f.id === 'hero');
const expected = { steps: 26, round: 12, winner: 0, health: 32, potions: 0 };
const actual = { steps: s.steps, round: s.round, winner: s.winner, health: hero.health, potions: hero.potions };
console.log('content warnings:', warnings, '| memory:', sim.memoryBytes(), 'bytes');
console.log('expected', JSON.stringify(expected));
console.log('actual  ', JSON.stringify(actual));
if (JSON.stringify(expected) !== JSON.stringify(actual)) {
    console.error('MISMATCH between WebAssembly and native results');
    process.exit(1);
}
console.log('OK: WebAssembly matches native');
