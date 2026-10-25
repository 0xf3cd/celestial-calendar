/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions between
 *   Gregorian and Chinese Lunar calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * SPDX-License-Identifier: MIT
 */

import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { readFile, readdir } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { BINDINGS, C_DECLARATIONS, CHART_CODES, LAYOUTS, PROTOCOLS, WASM_SIGNATURES } from "../../src/bindings.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const REPO = resolve(HERE, "../../../..");
const manifest = JSON.parse(await readFile(resolve(HERE, "manifest.json"), "utf8"));
const header = await readFile(resolve(REPO, "src/shared_lib/celestial.h"), "utf8");
const recipe = await readFile(resolve(REPO, "toolbox/build_wasm.py"), "utf8");
const recipeExports = JSON.parse(await readFile(resolve(REPO, "bindings/javascript/src/wasm_exports.json"), "utf8"));
const generated = spawnSync(process.env.PYTHON ?? "python3", [resolve(REPO, "toolbox/make_bindings.py"), "--check"], {
  encoding: "utf8",
});
assert.equal(generated.status, 0, `${generated.error ?? generated.stdout + generated.stderr}`);
assert.equal(createHash("sha256").update(header.replace(/\r\n?/g, "\n")).digest("hex"), C_DECLARATIONS.source_sha256);

const sameSet = (left, right) => assert.deepEqual([...new Set(left)].sort(), [...new Set(right)].sort());
const entries = Object.entries(C_DECLARATIONS.functions);
const names = entries.map(([name]) => name);
const readers = new Set(["celestial_last_error", "celestial_last_error_kind"]);
const recording = names.filter((name) => !readers.has(name));
assert.equal(names.length, 36);
assert.equal(recording.length, 34);
sameSet(names, BINDINGS.map(({ cName }) => cName));
sameSet(names, Object.keys(WASM_SIGNATURES));
sameSet([...names, "malloc", "free"], recipeExports);
assert.equal(recipeExports.length, 38);
assert(recipe.includes('"-sALLOW_MEMORY_GROWTH=1"'));
assert(recipe.includes('"HEAPU16"'));
assert.equal((recipe.match(/-sEXPORTED_FUNCTIONS=/g) ?? []).length, 1);

const label = (name) => {
  const [first, ...rest] = name.toLowerCase().split("_");
  return first + rest.map((part) => part[0].toUpperCase() + part.slice(1)).join("");
};
const verifyChartNames = (candidate) => {
  for (const [group, prefix] of [
    ["scales", "CELESTIAL_CIVIL_SCALE_"], ["models", "CELESTIAL_DELTA_T_MODEL_"],
    ["systems", "CELESTIAL_HOUSE_SYSTEM_"], ["present", "CELESTIAL_PRESENT_"], ["targets", "CELESTIAL_TARGET_"],
  ]) {
    const expected = Object.fromEntries(Object.entries(C_DECLARATIONS.constants)
      .filter(([name]) => name.startsWith(prefix) && name !== "CELESTIAL_TARGET_ALL")
      .map(([name, value]) => [label(name.slice(prefix.length)), value]));
    assert.deepEqual(candidate[group], expected);
  }
  assert.deepEqual(Object.values(candidate.targets), Array.from({ length: 14 }, (_, index) => index));
};
verifyChartNames(CHART_CODES);
for (const [group, first, second] of [
  ["targets", "mercury", "venus"], ["models", "algo1", "algo5"],
  ["systems", "equal", "wholeSign"], ["present", "latitude", "distance"],
]) {
  const changed = structuredClone(CHART_CODES);
  [changed[group][first], changed[group][second]] = [changed[group][second], changed[group][first]];
  assert.throws(() => verifyChartNames(changed), assert.AssertionError);
}

const emptyEvents = new Set(["celestial_longitude_events", "celestial_longitude_stations", "celestial_longitude_difference_events"]);
for (const [name, protocol] of Object.entries(PROTOCOLS)) {
  assert.equal(protocol.error_channel, readers.has(name) ? "preserve" : "kind");
  if (protocol.kind === "count-fill") {
    assert.equal(protocol.legal_empty, emptyEvents.has(name));
    assert.equal(protocol.total_must_match, true);
    assert.equal(protocol.written_policy, "min-total-capacity");
  }
  if (protocol.kind === "sized-string") {
    for (const property of ["size_includes_nul", "null_requires_zero_capacity", "truncates", "terminates"]) {
      assert.equal(protocol[property], true);
    }
  }
}
sameSet(Object.entries(PROTOCOLS).filter(([, p]) => p.kind === "sized-string").map(([name]) => name), [
  "celestial_get_jieqi_name", "celestial_capability_text", "celestial_model_identity_text",
]);

const verifyManifest = (candidate) => {
  assert.equal(candidate.exports.length, 36);
  sameSet(candidate.exports.map(({ name }) => name), names);
  assert.deepEqual(candidate.codes, C_DECLARATIONS.constants);
  assert.deepEqual(candidate.layouts, LAYOUTS);
  assert.equal(Object.keys(candidate.layouts).length, 28);
  assert.deepEqual(candidate.wasm_width_bits, {
    bool: 8, uint8_t: 8, uint16_t: 16, int32_t: 32, uint32_t: 32, pointer: 32, double: 64,
  });
  sameSet(candidate.exports.filter(({ recording }) => recording).map(({ name }) => name), recording);
  for (const entry of candidate.exports) {
    assert.equal(entry.signature, C_DECLARATIONS.functions[entry.name].signature);
    assert.deepEqual(entry.protocol, PROTOCOLS[entry.name]);
    const binding = BINDINGS.find(({ cName }) => cName === entry.name);
    assert.equal(entry.recording, binding.readsLastError);
    assert.equal(entry.return.kind === "sret", binding.result.startsWith("sret:"));
    if (entry.return.kind === "sret") assert.equal(binding.result, `sret:${entry.return.layout}`);
  }
};

const runMutationSelfTests = (candidate) => {
  const mutations = [];
  for (const [name, mutate] of [
    ["missing export", (m) => m.exports.pop()],
    ["wrong signature", (m) => { m.exports[0].signature = "uint32_t celestial_last_error(void)"; }],
    ["same-width field type", (m) => { m.layouts.CelestialLunarDate.fields[3].type = "uint8_t"; }],
    ["same-width event field type", (m) => { m.layouts.CelestialStationEventRecord.fields[3].type = "int32_t"; }],
    ["wrong offset", (m) => { m.layouts.CelestialJieqiMomentQuery.fields[1].offset = 2; }],
    ["wrong return kind", (m) => { m.exports.find(({ name }) => name === "celestial_moon_illumination").return.kind = "primitive"; }],
    ["wrong return layout", (m) => { m.exports.find(({ name }) => name === "celestial_moon_illumination").return.layout = "CelestialJulianDay"; }],
    ["missing recording marker", (m) => { m.exports.find(({ name }) => name === "celestial_moon_illumination").recording = false; }],
    ["recording reader", (m) => { m.exports[0].recording = true; }],
    ["target code", (m) => { m.codes.CELESTIAL_TARGET_PLUTO = 10; }],
    ["presence bit", (m) => { m.codes.CELESTIAL_PRESENT_DISTANCE = 4; }],
    ["missing code", (m) => { delete m.codes.CELESTIAL_PRESENT_DISTANCE; }],
    ["extra code", (m) => { m.codes.CELESTIAL_TARGET_CERES = 14; }],
  ]) {
    const changed = structuredClone(candidate);
    mutate(changed);
    mutations.push([name, changed]);
  }
  for (const name of ["celestial_moon_phase_moments", "celestial_new_moons_in_year", "celestial_new_moons_after_jde",
    "celestial_longitude_events", "celestial_longitude_stations", "celestial_longitude_difference_events"]) {
    const changed = structuredClone(candidate);
    changed.exports.find((entry) => entry.name === name).protocol.kind = "companion-fill";
    mutations.push([`wrong fill ${name}`, changed]);
  }
  assert.equal(mutations.length, 19);
  for (const [name, changed] of mutations) assert.throws(() => verifyManifest(changed), assert.AssertionError, name);
};
verifyManifest(manifest);
runMutationSelfTests(manifest);

if (!process.argv.includes("--static-only")) {
  const sourceDir = resolve(REPO, "src/shared_lib");
  const files = (await readdir(sourceDir)).filter((name) => /^lib.*\.cpp$/.test(name));
  const sources = (await Promise.all(files.map((name) => readFile(resolve(sourceDir, name), "utf8")))).join("\n");
  for (const name of names) {
    const start = sources.search(new RegExp(`auto\\s+${name}\\s*\\(`));
    assert(start >= 0, `missing implementation: ${name}`);
    const open = sources.indexOf("{", start);
    let depth = 0;
    let end = open;
    do { if (sources[end] === "{") ++depth; if (sources[end] === "}") --depth; ++end; } while (depth && end < sources.length);
    assert.equal(sources.slice(open, end).includes("lib::wrap_export("), !readers.has(name), `recording ${name}`);
  }
  const M = await (await import(pathToFileURL(resolve(REPO, "build/wasm/celestial-jieqi.mjs")))).default();
  const built = Object.keys(M).filter((name) => /^_[a-z]/.test(name) && typeof M[name] === "function").map((name) => name.slice(1));
  sameSet(recipeExports, built);
  assert(M.HEAPU16 instanceof Uint16Array);
  console.log("PASS source writers and exact built WASM export inventory");
} else {
  console.log("Static-only: source implementations and built WASM checks not run");
}
console.log("PASS declarations 36, records 28, recording 34, ABI mutations 19/19, code swaps 4/4");
console.log("PASS generated legal-empty, exact-total, sized-string and preserving-reader policies");
