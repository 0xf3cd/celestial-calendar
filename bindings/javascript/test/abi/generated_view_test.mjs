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
import { BINDINGS, C_DECLARATIONS, CHART_CODES, LAYOUTS, PROTOCOLS, WASM_SIGNATURES } from "../../src/bindings.mjs";
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
  assert.equal(names.length, 43);
  assert.equal(Object.keys(candidate.layouts).length, 33);
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
  assert.equal(candidate.exports.filter(({ protocol }) => protocol.kind === "count-fill").length, 12);
  assert.equal(candidate.exports.filter(({ recording }) => recording).length, 41);
  assert.deepEqual(candidate.exports.filter(({ protocol }) => protocol.error_channel === "preserve")
    .map(({ name }) => name), ["celestial_last_error", "celestial_last_error_kind"]);
  const targetCodes = Object.entries(candidate.codes)
    .filter(([name]) => name.startsWith("CELESTIAL_TARGET_") && name !== "CELESTIAL_TARGET_ALL")
    .sort((left, right) => left[1] - right[1])
    .map(([name, value]) => [chartLabel(name.slice("CELESTIAL_TARGET_".length)), value]);
  assert.deepEqual(Object.entries(CHART_CODES.targets), targetCodes);
};

assertView(manifest);
assert.equal(LAYOUTS.CelestialApparentDiameter.size, 64);
assert.equal(LAYOUTS.CelestialApparentDiameter.alignment, 8);
assert.deepEqual(LAYOUTS.CelestialApparentDiameter.fields.map(({ name, type, offset }) => [name, type, offset]), [
  ["valid", "bool", 0], ["target_code", "uint32_t", 4], ["centre_code", "uint32_t", 8],
  ["source_code", "uint32_t", 12], ["radius_definition_code", "uint32_t", 16],
  ["delta_t_identity_code", "uint32_t", 20], ["jd_ut1", "double", 24], ["jde_tt", "double", 32],
  ["radius_km", "double", 40], ["distance_au", "double", 48], ["diameter_arcsec", "double", 56],
]);
assert.deepEqual(WASM_SIGNATURES.celestial_apparent_diameter, {
  parameters: ["i32", "i32", "i32", "i32", "i32", "f64", "i32", "i32", "i32", "f64", "f64", "f64"],
  result: "void",
});
// Independent wasm32 pins; the declaration generator must not rewrite this expectation.
assert.equal(LAYOUTS.CelestialTopocentricPosition.size, 88);
assert.equal(LAYOUTS.CelestialTopocentricPosition.alignment, 8);
assert.deepEqual(LAYOUTS.CelestialTopocentricPosition.fields.map(({ name, type, offset }) => [name, type, offset]), [
  ["valid", "bool", 0], ["target_code", "uint32_t", 4], ["source_code", "uint32_t", 8],
  ["delta_t_identity_code", "uint32_t", 12], ["present_fields", "uint32_t", 16],
  ["jd_ut1", "double", 24], ["jde_tt", "double", 32], ["right_ascension_deg", "double", 40],
  ["declination_deg", "double", 48], ["distance_au", "double", 56], ["altitude_deg", "double", 64],
  ["azimuth_deg", "double", 72], ["apparent_altitude_deg", "double", 80],
]);
assert.deepEqual(exportsList, [...names, "malloc", "free"], "exact generated private WASM export inventory");
assert.equal(LAYOUTS.CelestialAltitudeCondition.size, 40);
assert.equal(LAYOUTS.CelestialAltitudeCondition.alignment, 8);
assert.deepEqual(LAYOUTS.CelestialAltitudeCondition.fields.map(({ name, type, offset }) => [name, type, offset]), [
  ["target_code", "uint32_t", 0], ["sense_code", "uint32_t", 4], ["refraction_code", "uint32_t", 8],
  ["reserved", "uint32_t", 12], ["level_deg", "double", 16], ["temperature_c", "double", 24], ["pressure_hpa", "double", 32],
]);
assert.equal(LAYOUTS.CelestialAltitudeRecord.size, 88);
assert.equal(LAYOUTS.CelestialAltitudeRecord.alignment, 8);
assert.deepEqual(LAYOUTS.CelestialAltitudeRecord.fields.map(({ name, type, offset }) => [name, type, offset]), [
  ["tag", "uint32_t", 0], ["condition_index", "uint32_t", 4], ["target_code", "uint32_t", 8],
  ["position_source_code", "uint32_t", 12], ["delta_t_model_code", "uint32_t", 16], ["refraction_code", "uint32_t", 20],
  ["sense_code", "uint32_t", 24], ["direction_code", "uint32_t", 28], ["initially_satisfied", "uint32_t", 32],
  ["reserved", "uint32_t", 36], ["jde_tt", "double", 40], ["end_jde_tt", "double", 48],
  ["requested_level_deg", "double", 56], ["geometric_level_deg", "double", 64],
  ["temperature_c", "double", 72], ["pressure_hpa", "double", 80],
]);
assert.deepEqual(WASM_SIGNATURES.celestial_altitude_events, {
  parameters: ["i32", "f64", "f64", "f64", "f64", "f64", "i32", "i32", "i32"], result: "i32",
});
assert.deepEqual(WASM_SIGNATURES.celestial_upper_transits, WASM_SIGNATURES.celestial_altitude_events);
assert.deepEqual(WASM_SIGNATURES.celestial_observing_windows, {
  parameters: ["i32", "i32", "f64", "f64", "f64", "f64", "f64", "i32", "i32", "i32"], result: "i32",
});
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

const assertRawDecoders = (source) => {
  const records = [...source.matchAll(/"(Celestial[A-Z][A-Za-z0-9]*)"/g)].map((match) => match[1]);
  assert(records.length > 0, "raw decoder record names must be visible to the check");
  for (const record of records) assert(Object.hasOwn(LAYOUTS, record), `unknown raw decoder record: ${record}`);
  const calls = [...source.matchAll(/eventFill\(\s*"(celestial_\w+)",\s*"(Celestial\w+)"/g)];
  assert(calls.length > 0, "literal count/fill decoders must be visible to the check");
  for (const [, name, record] of calls) {
    const protocol = PROTOCOLS[name];
    assert.equal(protocol?.kind, "count-fill", `${name} decoder protocol`);
    const slots = C_DECLARATIONS.functions[name].parameters.find(({ name }) => name === protocol.slots_parameter);
    assert.equal(slots?.type, `${record} *`, `${name} raw decoder pointee`);
  }
};
const rawSource = await readFile(new URL("./raw_protocol_test.mjs", import.meta.url), "utf8");
assertRawDecoders(rawSource);
for (const replacement of ["CelestialEquatorialPosition", "CelestialPositionRecord"]) {
  const changed = rawSource.replace('"CelestialEquatorialPositionRecord"', `"${replacement}"`);
  assert.notEqual(changed, rawSource);
  assert.throws(() => assertRawDecoders(changed), assert.AssertionError);
}
console.log(`PASS generated JavaScript schema@2: ${names.length} exports, ${Object.keys(LAYOUTS).length} layouts, ${Object.keys(manifest.codes).length} named codes, 12 counted protocols; ${mutations.length}/${mutations.length} drift controls`);
console.log("PASS literal raw decoder record/pointee names; 2/2 corruption controls");
