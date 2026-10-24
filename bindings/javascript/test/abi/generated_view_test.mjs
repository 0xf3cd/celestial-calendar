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
import { readFile } from "node:fs/promises";
import { BINDINGS, C_DECLARATIONS, CHART_CODES, LAYOUTS, PROTOCOLS } from "../../src/bindings.mjs";
import { chartLabel } from "../support/chart_replay.mjs";

const manifest = JSON.parse(await readFile(new URL("./manifest.json", import.meta.url), "utf8"));
const exportsList = JSON.parse(await readFile(new URL("../../src/wasm_exports.json", import.meta.url), "utf8"));
const names = BINDINGS.map(({ cName }) => cName);
const assertView = (candidate) => {
  assert.equal(candidate.schema, "celestial-calendar/javascript-wasm-abi@2");
  assert.deepEqual(Object.keys(candidate).sort(), ["schema", "source_header", "wasm_width_bits", "layouts", "codes", "exports"].sort());
  assert.equal(candidate.source_header, "src/shared_lib/celestial.h");
  assert.equal(candidate.wasm_width_bits.pointer, 32);
  assert.deepEqual(candidate.codes, C_DECLARATIONS.constants, "every named macro, not only chart codes");
  assert(Object.entries(candidate.codes).every(([name, value]) => name.startsWith("CELESTIAL_")
    && Number.isInteger(value) && value >= 0 && value <= 0xFFFFFFFF), "full-width code values");
  assert.deepEqual(candidate.layouts, LAYOUTS);
  assert.deepEqual(candidate.exports.map(({ name }) => name), names);
  assert.deepEqual(Object.keys(PROTOCOLS), names);
  assert.deepEqual(Object.keys(C_DECLARATIONS.functions), names);
  assert.equal(names.length, 36);
  assert.equal(Object.keys(candidate.layouts).length, 28);
  assert(names.every((name) => name.startsWith("celestial_")));
  assert(Object.keys(candidate.layouts).every((name) => name.startsWith("Celestial") && !name.endsWith("V1")));
  for (const [index, entry] of candidate.exports.entries()) {
    const binding = BINDINGS[index];
    const declaration = C_DECLARATIONS.functions[entry.name];
    assert.equal(entry.signature, declaration.signature, `${entry.name} signature`);
    assert.equal(entry.recording, binding.readsLastError, `${entry.name} recording classification`);
    assert.deepEqual(entry.protocol, PROTOCOLS[entry.name], `${entry.name} protocol`);
    assert.equal(binding.wasmName, `_${entry.name}`);
    const expectedResult = entry.return.kind === "sret" ? `sret:${entry.return.layout}`
      : entry.protocol.kind === "primitive" ? entry.return.type : entry.protocol.kind;
    assert.equal(binding.result, expectedResult, `${entry.name} result`);
    if (entry.protocol.kind === "count-fill") {
      const protocol = entry.protocol;
      assert.equal(typeof protocol.legal_empty, "boolean");
      assert.equal(protocol.total_must_match, true);
      assert.equal(protocol.written_policy, "min-total-capacity");
      assert.equal(protocol.null_slots_requires_zero_capacity, true);
      assert.equal(protocol.error_channel, "kind");
      assert.deepEqual(
        [protocol.count_parameter, protocol.slots_parameter, protocol.capacity_parameter],
        declaration.parameters.slice(-3).map(({ name }) => name),
        `${entry.name} count/fill parameter roles`,
      );
    }
  }
  assert.equal(candidate.exports.filter(({ protocol }) => protocol.kind === "count-fill").length, 9);
  assert.equal(candidate.exports.filter(({ recording }) => recording).length, 34);
  assert.deepEqual(candidate.exports.filter(({ protocol }) => protocol.error_channel === "preserve")
    .map(({ name }) => name), ["celestial_last_error", "celestial_last_error_kind"]);
  const targetCodes = Object.entries(candidate.codes)
    .filter(([name]) => name.startsWith("CELESTIAL_TARGET_") && name !== "CELESTIAL_TARGET_ALL")
    .sort((left, right) => left[1] - right[1])
    .map(([name, value]) => [chartLabel(name.slice("CELESTIAL_TARGET_".length)), value]);
  assert.deepEqual(Object.entries(CHART_CODES.targets), targetCodes);
};

assertView(manifest);
assert.deepEqual(exportsList, [...names, "malloc", "free"], "exact generated private WASM export inventory");
const mutations = [
  (value) => { value.schema = "celestial-calendar/javascript-wasm-abi@1"; },
  (value) => { value.chart_v1_codes = value.codes; delete value.codes; },
  (value) => { delete value.codes.CELESTIAL_ERROR_INTERNAL; },
  (value) => { value.codes.CELESTIAL_ERROR_INVALID_INPUT = 2; },
  (value) => { value.exports.find(({ protocol }) => protocol.kind === "count-fill").protocol.legal_empty = "false"; },
  (value) => { value.exports.find(({ protocol }) => protocol.kind === "count-fill").protocol.total_must_match = false; },
  (value) => { delete value.exports[0].protocol; },
  (value) => { value.exports.pop(); },
  (value) => { value.exports.find(({ return: result }) => result.kind === "sret").return.layout = "CelestialDeltaT"; },
  (value) => { value.exports[0].recording = true; },
  (value) => { value.exports[0].signature += " const"; },
  (value) => { ++value.layouts.CelestialChartBody.fields.find(({ name }) => name === "source_code").offset; },
];
for (const mutate of mutations) {
  const candidate = structuredClone(manifest);
  mutate(candidate);
  assert.throws(() => assertView(candidate), assert.AssertionError);
}
console.log(`PASS generated JavaScript schema@2: ${names.length} exports, ${Object.keys(LAYOUTS).length} layouts, ${Object.keys(manifest.codes).length} named codes, 9 counted protocols; ${mutations.length}/${mutations.length} drift controls`);
