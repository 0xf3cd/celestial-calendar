/*
 * CelestialCalendar:
 *   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
 *   including Gregorian, Lunar, and Chinese Ganzhi calendars.
 *
 * Copyright (C) 2026 Ningqi Wang (0xf3cd)
 * Email: nq.maigre@gmail.com
 * Repo : https://github.com/0xf3cd/celestial-calendar
 *
 * This project is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This project is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this project. If not, see <https://www.gnu.org/licenses/>.
 */

import assert from "node:assert/strict";
import { readFile, readdir } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

import { BINDINGS, LAYOUTS } from "../../src/bindings.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const REPO = resolve(HERE, "../../../..");
const HEADER_PATH = resolve(REPO, "src/shared_lib/celestial.h");
const BUILD_SCRIPT_PATH = resolve(REPO, "toolbox/build_wasm.py");
const MODULE_PATH = resolve(REPO, "build/wasm/celestial-jieqi.mjs");

const manifest = JSON.parse(await readFile(resolve(HERE, "manifest.json"), "utf8"));
const header = await readFile(HEADER_PATH, "utf8");
const buildScript = await readFile(BUILD_SCRIPT_PATH, "utf8");

const sorted = (values) => [...values].sort();
const sameSet = (label, left, right) => {
  assert.deepEqual(sorted(new Set(left)), sorted(new Set(right)), label);
};
const uniqueCount = (label, values, expected) => {
  assert.equal(values.length, expected, `${label} count`);
  assert.equal(new Set(values).size, expected, `${label} unique count`);
};
const canonical = (value) => value
  .replace(/\s+/g, " ")
  .replace(/\s*\*\s*/g, " *")
  .replace(/\s*\(\s*/g, "(")
  .replace(/\s*\)\s*/g, ")")
  .replace(/\s*,\s*/g, ", ")
  .trim();

const declarationRegion = header.slice(header.indexOf("/* ---------- Global configuration ---------- */"));
const declarations = [...declarationRegion.matchAll(/CELESTIAL_API\s+([^;]+);/g)].map((match) => match[1]);
const headerEntries = declarations.map((signature) => {
  const name = signature.match(/([A-Za-z_]\w*)\s*\(/)?.[1];
  assert(name, `cannot parse declaration: ${signature}`);
  return { name, signature };
});
const headerNames = headerEntries.map(({ name }) => name);
const manifestNames = manifest.exports.map(({ name }) => name);
const bindingNames = BINDINGS.map(({ cName }) => cName);

uniqueCount("celestial.h exports", headerNames, 29);
uniqueCount("manifest exports", manifestNames, 29);
uniqueCount("internal bindings", bindingNames, 29);
sameSet("header = manifest", headerNames, manifestNames);
sameSet("header = bindings", headerNames, bindingNames);

for (const entry of manifest.exports) {
  const declared = headerEntries.find(({ name }) => name === entry.name);
  assert(declared, `missing header declaration for ${entry.name}`);
  assert.equal(canonical(entry.signature), canonical(declared.signature), `signature ${entry.name}`);

  const bindingEntry = BINDINGS.find(({ cName }) => cName === entry.name);
  assert(bindingEntry, `missing binding for ${entry.name}`);
  if (entry.return.kind === "sret") {
    assert.equal(bindingEntry.result, `sret:${entry.return.layout}`, `binding result ${entry.name}`);
  }
}

const expectedWidths = {
  bool: 8,
  uint8_t: 8,
  uint16_t: 16,
  int32_t: 32,
  uint32_t: 32,
  pointer: 32,
  double: 64,
};
assert.deepEqual(manifest.wasm_width_bits, expectedWidths, "WASM integer/pointer widths");

const typeLayout = {
  bool: { size: 1, alignment: 1 },
  uint8_t: { size: 1, alignment: 1 },
  uint16_t: { size: 2, alignment: 2 },
  int32_t: { size: 4, alignment: 4 },
  uint32_t: { size: 4, alignment: 4 },
  double: { size: 8, alignment: 8 },
};
const alignTo = (value, alignment) => Math.ceil(value / alignment) * alignment;
const parsedLayouts = {};
for (const match of header.matchAll(/typedef struct (\w+)\s*\{([\s\S]*?)\}\s*\1;/g)) {
  const [, name, rawBody] = match;
  const body = rawBody.replace(/\/\*[\s\S]*?\*\//g, "");
  const fields = [...body.matchAll(/\b(bool|uint8_t|uint16_t|int32_t|uint32_t|double)\s+(\w+)\s*;/g)]
    .map((field) => ({ name: field[2], type: field[1] }));
  let offset = 0;
  let alignment = 1;
  for (const field of fields) {
    const type = typeLayout[field.type];
    offset = alignTo(offset, type.alignment);
    field.offset = offset;
    offset += type.size;
    alignment = Math.max(alignment, type.alignment);
  }
  parsedLayouts[name] = { size: alignTo(offset, alignment), alignment, fields };
}
const manifestLayoutNames = Object.keys(manifest.layouts);
uniqueCount("manifest layouts", manifestLayoutNames, 16);
sameSet("header layouts = manifest layouts", Object.keys(parsedLayouts), manifestLayoutNames);
for (const name of manifestLayoutNames) {
  assert.deepEqual(manifest.layouts[name], parsedLayouts[name], `layout ${name}`);
}
assert.deepEqual(LAYOUTS, manifest.layouts, "runtime layouts = manifest layouts");

const listValues = (constantName) => {
  const block = buildScript.match(new RegExp(`${constantName}: Final\\[list\\[str\\]\\] = \\[([\\s\\S]*?)\\n\\]`));
  assert(block, `cannot parse ${constantName} from build_wasm.py`);
  return [...block[1].matchAll(/"([A-Za-z0-9_]+)"/g)].map((match) => match[1]);
};
const recipeExports = listValues("EXPORTS");
const runtimeMethods = listValues("RUNTIME_METHODS");
uniqueCount("build recipe exports", recipeExports, 31);
sameSet("manifest + malloc/free = build recipe", [...manifestNames, "malloc", "free"], recipeExports);
assert(runtimeMethods.includes("HEAPU16"), "build recipe must export HEAPU16");
assert(buildScript.includes('"-sALLOW_MEMORY_GROWTH=1"'), "build recipe must enable ALLOW_MEMORY_GROWTH");
assert.equal((buildScript.match(/-sEXPORTED_FUNCTIONS=/g) ?? []).length, 1, "one em++ export recipe");

const M = await (await import(pathToFileURL(MODULE_PATH))).default();
for (const name of recipeExports) {
  assert.equal(typeof M[`_${name}`], "function", `built module export _${name}`);
}
const builtExports = Object.keys(M)
  .filter((name) => /^_[a-z]/.test(name) && typeof M[name] === "function")
  .map((name) => name.slice(1));
sameSet("build recipe = built module", recipeExports, builtExports);
assert(M.HEAPU16 instanceof Uint16Array, "built module runtime HEAPU16");

const recordingParagraph = header.match(/Only the recording functions \(([\s\S]*?)\) write and clear the message/);
assert(recordingParagraph, "cannot parse celestial.h recording list");
const documentedRecording = [...recordingParagraph[1].matchAll(/`([a-z0-9_]+)`/g)].map((match) => match[1]);

const sourceDir = resolve(REPO, "src/shared_lib");
const sourceNames = (await readdir(sourceDir)).filter((name) => /^lib.*\.cpp$/.test(name));
const sources = (await Promise.all(sourceNames.map((name) => readFile(resolve(sourceDir, name), "utf8")))).join("\n");
const functionBody = (name) => {
  const start = sources.search(new RegExp(`auto\\s+${name}\\s*\\(`));
  assert(start >= 0, `missing implementation for ${name}`);
  const open = sources.indexOf("{", start);
  let depth = 0;
  for (let index = open; index < sources.length; ++index) {
    if (sources[index] === "{") ++depth;
    if (sources[index] === "}" && --depth === 0) return sources.slice(open, index + 1);
  }
  assert.fail(`unterminated implementation for ${name}`);
};
const implementationWriters = headerNames.filter((name) => functionBody(name).includes("lib::clear_last_error()"));
const manifestRecording = manifest.exports.filter(({ recording }) => recording).map(({ name }) => name);
const bindingErrorPolicy = BINDINGS.filter(({ readsLastError }) => readsLastError).map(({ cName }) => cName);

uniqueCount("recording exports", documentedRecording, 7);
sameSet("recording docs = implementation writers", documentedRecording, implementationWriters);
sameSet("recording docs = manifest", documentedRecording, manifestRecording);
sameSet("recording docs = binding error policy", documentedRecording, bindingErrorPolicy);

console.log("PASS exports header=manifest=bindings=recipe=built 29 (+ malloc/free)");
console.log("PASS layouts header=manifest 16; HEAPU16 present; memory growth enabled");
console.log("PASS recording docs=writers=manifest=binding error policy 7");
