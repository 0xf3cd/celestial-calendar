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
const bindingNames = BINDINGS.map(({ cName }) => cName);

uniqueCount("celestial.h exports", headerNames, 30);
uniqueCount("internal bindings", bindingNames, 30);
sameSet("header = bindings", headerNames, bindingNames);

const expectedWidths = {
  bool: 8,
  uint8_t: 8,
  uint16_t: 16,
  int32_t: 32,
  uint32_t: 32,
  pointer: 32,
  double: 64,
};

const typeLayout = {
  bool: { size: 1, alignment: 1 },
  uint8_t: { size: 1, alignment: 1 },
  uint16_t: { size: 2, alignment: 2 },
  int32_t: { size: 4, alignment: 4 },
  uint32_t: { size: 4, alignment: 4 },
  double: { size: 8, alignment: 8 },
};
const alignTo = (value, alignment) => {
  const aligned = Math.ceil(value / alignment) * alignment;
  assert(Number.isSafeInteger(aligned), `unsafe layout size ${aligned}`);
  return aligned;
};
const parseHeaderLayouts = (source) => {
  const clean = source.replace(/\/\*[\s\S]*?\*\//g, " ").replace(/\/\/[^\n]*/g, " ");
  const recordPattern = /\btypedef\s+struct\s+([A-Za-z_]\w*)\s*\{([^{}]*)\}\s*([A-Za-z_]\w*)\s*;/g;
  const layouts = {};
  for (const [, name, body, alias] of clean.matchAll(recordPattern)) {
    assert.equal(alias, name, `unsupported record alias ${name} -> ${alias}`);
    assert(!Object.hasOwn(layouts, name) && !Object.hasOwn(typeLayout, name), `duplicate record ${name}`);

    const declarations = body.split(";");
    assert.equal(declarations.pop().trim(), "", `unterminated field in ${name}`);
    assert(declarations.length > 0, `empty record ${name}`);
    const fields = [];
    let offset = 0;
    let alignment = 1;
    for (const declaration of declarations) {
      // Closed grammar: scalars, earlier named records, and fixed decimal-literal arrays.
      const field = declaration.trim().match(/^([A-Za-z_]\w*)\s+([A-Za-z_]\w*)(?:\s*\[\s*([1-9][0-9]*)\s*\])?$/);
      assert(field, `unsupported C field in ${name}: ${declaration.trim()}`);
      const [, baseName, fieldName, extent] = field;
      const base = Object.hasOwn(typeLayout, baseName) ? typeLayout[baseName]
        : Object.hasOwn(layouts, baseName) ? layouts[baseName] : undefined;
      assert(base, `unknown or forward C field type ${name}.${fieldName}: ${baseName}`);
      assert(!fields.some(({ name }) => name === fieldName), `duplicate field ${name}.${fieldName}`);

      const count = extent === undefined ? 1 : Number(extent);
      const size = base.size * count;
      assert(Number.isSafeInteger(count) && count > 0 && Number.isSafeInteger(size), `unsafe array extent ${name}.${fieldName}`);
      offset = alignTo(offset, base.alignment);
      fields.push({ name: fieldName, type: extent === undefined ? baseName : `${baseName}[${count}]`, offset });
      offset += size;
      assert(Number.isSafeInteger(offset), `unsafe field end ${name}.${fieldName}`);
      alignment = Math.max(alignment, base.alignment);
    }
    layouts[name] = { size: alignTo(offset, alignment), alignment, fields };
  }
  assert(!/\btypedef\s+struct\b/.test(clean.replace(recordPattern, "")), "unsupported C record declaration");
  return layouts;
};

const parseChartCodes = (source) => {
  const clean = source.replace(/"(?:\\[\s\S]|[^"\\])*"|'(?:\\[\s\S]|[^'\\])*'|\/\*[\s\S]*?\*\/|\/\/[^\n]*/g, " ");
  assert(!/[^\x00-\x7f]|\\|##|%:/.test(clean), "non-ASCII, backslash or token paste in chart code text");
  const tokens = new Set(clean.match(/CHART_\w*/g));
  const codes = {};
  for (const [, declaration] of clean.matchAll(/^\s*#define\s+(CHART_.*)$/gm)) {
    const match = declaration.trim().match(/^(CHART_\w+)\s+UINT32_C\(([0-9]+)\)$/);
    assert(match, `unsupported chart code declaration: ${declaration}`);
    assert(!Object.hasOwn(codes, match[1]), `duplicate chart code: ${match[1]}`);
    codes[match[1]] = Number(match[2]);
  }
  assert(Object.keys(codes).length, "missing chart codes");
  sameSet("closed chart code tokens", tokens, Object.keys(codes));
  return codes;
};

const parsedLayouts = parseHeaderLayouts(header);
const parsedCodes = parseChartCodes(header);

const listValues = (constantName) => {
  const block = buildScript.match(new RegExp(`${constantName}: Final\\[list\\[str\\]\\] = \\[([\\s\\S]*?)\\n\\]`));
  assert(block, `cannot parse ${constantName} from build_wasm.py`);
  return [...block[1].matchAll(/"([A-Za-z0-9_]+)"/g)].map((match) => match[1]);
};
const recipeExports = listValues("EXPORTS");
const runtimeMethods = listValues("RUNTIME_METHODS");
uniqueCount("build recipe exports", recipeExports, 32);
assert(runtimeMethods.includes("HEAPU16"), "build recipe must export HEAPU16");
assert(buildScript.includes('"-sALLOW_MEMORY_GROWTH=1"'), "build recipe must enable ALLOW_MEMORY_GROWTH");
assert.equal((buildScript.match(/-sEXPORTED_FUNCTIONS=/g) ?? []).length, 1, "one em++ export recipe");

assert(
  header.includes("Every exported function except `last_error` writes and clears the message"),
  "celestial.h recording contract",
);
const documentedRecording = headerNames.filter((name) => name !== "last_error");

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
const implementationWriters = headerNames.filter((name) => functionBody(name).includes("lib::wrap_export("));
const bindingErrorPolicy = BINDINGS.filter(({ readsLastError }) => readsLastError).map(({ cName }) => cName);

uniqueCount("recording exports", documentedRecording, 29);
sameSet("recording docs = implementation writers", documentedRecording, implementationWriters);
sameSet("recording docs = binding error policy", documentedRecording, bindingErrorPolicy);

const verifyManifest = (candidate) => {
  const manifestNames = candidate.exports.map(({ name }) => name);
  uniqueCount("manifest exports", manifestNames, 30);
  sameSet("header = manifest", headerNames, manifestNames);

  for (const entry of candidate.exports) {
    const declared = headerEntries.find(({ name }) => name === entry.name);
    assert(declared, `missing header declaration for ${entry.name}`);
    assert.equal(canonical(entry.signature), canonical(declared.signature), `signature ${entry.name}`);

    const bindingEntry = BINDINGS.find(({ cName }) => cName === entry.name);
    assert(bindingEntry, `missing binding for ${entry.name}`);
    assert.equal(entry.return.kind === "sret", bindingEntry.result.startsWith("sret:"), `return kind ${entry.name}`);
    if (entry.return.kind === "sret") {
      assert.equal(bindingEntry.result, `sret:${entry.return.layout}`, `binding result ${entry.name}`);
    }
    if (bindingEntry.result.endsWith("-fill") || entry.protocol.kind.endsWith("-fill")) {
      assert.equal(entry.protocol.kind, bindingEntry.result, `fill protocol ${entry.name}`);
    }
  }

  assert.deepEqual(candidate.wasm_width_bits, expectedWidths, "WASM integer/pointer widths");

  const manifestLayoutNames = Object.keys(candidate.layouts);
  uniqueCount("manifest layouts", manifestLayoutNames, 19);
  sameSet("header layouts = manifest layouts", Object.keys(parsedLayouts), manifestLayoutNames);
  for (const name of manifestLayoutNames) {
    assert.deepEqual(candidate.layouts[name], parsedLayouts[name], `layout ${name}`);
  }
  assert.deepEqual(LAYOUTS, candidate.layouts, "runtime layouts = manifest layouts");
  assert.deepEqual(candidate.chart_v1_codes, parsedCodes, "header = manifest chart codes");

  sameSet("manifest + malloc/free = build recipe", [...manifestNames, "malloc", "free"], recipeExports);
  const manifestRecording = candidate.exports.filter(({ recording }) => recording).map(({ name }) => name);
  sameSet("recording docs = manifest", documentedRecording, manifestRecording);
};

const runMutationSelfTests = (candidate) => {
  const mutations = [];

  const missingExport = structuredClone(candidate);
  missingExport.exports.pop();
  mutations.push(["missing export", missingExport]);

  const wrongSignature = structuredClone(candidate);
  wrongSignature.exports.find(({ name }) => name === "delta_t").signature = "DeltaT delta_t(int32_t year)";
  mutations.push(["wrong signature", wrongSignature]);

  const wrongFieldType = structuredClone(candidate);
  wrongFieldType.layouts.LunarDate.fields[3].type = "uint8_t";
  mutations.push(["same-width field type", wrongFieldType]);

  const wrongOffset = structuredClone(candidate);
  wrongOffset.layouts.JieqiMomentQuery.fields[1].offset = 2;
  mutations.push(["wrong offset", wrongOffset]);

  const wrongReturnKind = structuredClone(candidate);
  wrongReturnKind.exports.find(({ name }) => name === "moon_illumination").return.kind = "primitive";
  mutations.push(["wrong return kind", wrongReturnKind]);

  const wrongReturnLayout = structuredClone(candidate);
  wrongReturnLayout.exports.find(({ name }) => name === "moon_illumination").return.layout = "JulianDay";
  mutations.push(["wrong return layout", wrongReturnLayout]);

  const wrongRecording = structuredClone(candidate);
  wrongRecording.exports.find(({ name }) => name === "moon_illumination").recording = false;
  mutations.push(["missing recording marker", wrongRecording]);

  const recordingReader = structuredClone(candidate);
  recordingReader.exports.find(({ name }) => name === "last_error").recording = true;
  mutations.push(["recording last_error", recordingReader]);

  for (const name of ["moon_phase_moments", "new_moons_in_year", "new_moons_after_jde"]) {
    const swappedFill = structuredClone(candidate);
    const entry = swappedFill.exports.find((entry) => entry.name === name);
    const companion = swappedFill.exports.find(({ name }) => name === "solar_lon_roots");
    [entry.protocol.kind, companion.protocol.kind] = [companion.protocol.kind, entry.protocol.kind];
    sameSet(
      "swapped fill kinds preserve the category set",
      swappedFill.exports.map(({ protocol }) => protocol.kind),
      candidate.exports.map(({ protocol }) => protocol.kind),
    );
    mutations.push([`swapped fill ownership ${name}`, swappedFill]);
  }

  const wrongIdentityCode = structuredClone(candidate);
  wrongIdentityCode.chart_v1_codes.CHART_TARGET_PLUTO = 10;
  mutations.push(["chart identity code", wrongIdentityCode]);

  const wrongPresence = structuredClone(candidate);
  wrongPresence.chart_v1_codes.CHART_PRESENT_DISTANCE = 4;
  mutations.push(["chart presence bit", wrongPresence]);

  const missingCode = structuredClone(candidate);
  delete missingCode.chart_v1_codes.CHART_PRESENT_DISTANCE;
  mutations.push(["missing chart code", missingCode]);

  const extraCode = structuredClone(candidate);
  extraCode.chart_v1_codes.CHART_TARGET_CERES = 14;
  mutations.push(["extra chart code", extraCode]);

  assert.equal(mutations.length, 15, "ABI mutation denominator");
  for (const [label, mutated] of mutations) {
    assert.throws(() => verifyManifest(mutated), assert.AssertionError, `ABI gate accepted mutation: ${label}`);
  }
};

verifyManifest(manifest);
runMutationSelfTests(manifest);

const M = await (await import(pathToFileURL(MODULE_PATH))).default();
for (const name of recipeExports) {
  assert.equal(typeof M[`_${name}`], "function", `built module export _${name}`);
}
const builtExports = Object.keys(M)
  .filter((name) => /^_[a-z]/.test(name) && typeof M[name] === "function")
  .map((name) => name.slice(1));
sameSet("build recipe = built module", recipeExports, builtExports);
assert(M.HEAPU16 instanceof Uint16Array, "built module runtime HEAPU16");

console.log("PASS exports header=manifest=bindings=recipe=built 30 (+ malloc/free); HEAPU16 present");
console.log("PASS layouts header=manifest=bindings 19; memory growth enabled in recipe");
console.log("PASS recording docs=writers=manifest=binding error policy 29");
console.log("PASS per-export fill protocols; ABI mutations rejected 15/15");
