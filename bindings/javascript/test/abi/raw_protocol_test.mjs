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
import { dirname, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

import { BINDINGS } from "../../src/bindings.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const REPO = resolve(HERE, "../../../..");
const manifest = JSON.parse(await readFile(resolve(HERE, "manifest.json"), "utf8"));
const golden = JSON.parse(await readFile(resolve(REPO, "toolbox/bindings_golden.json"), "utf8"));
const M = await (await import(pathToFileURL(resolve(REPO, "build/wasm/celestial-jieqi.mjs")))).default();

assert.equal(golden.schema, "celestial-calendar/bindings-golden@2");
assert.deepEqual(Object.keys(golden.provenance).sort(), ["generated_on", "seed", "source_commit"]);
assert.match(golden.provenance.source_commit, /^[0-9a-f]{40}$/);
assert.equal(typeof golden.provenance.generated_on, "string");
assert(golden.provenance.generated_on.length > 0);
assert.equal(golden.provenance.seed, 42);
assert.deepEqual(
  Object.fromEntries(Object.entries(golden.sections).map(([name, section]) => [name, section.entries.length])),
  { jieqi: 204, moon: 41, sidereal: 43, moon_position_angle: 41, phases: 60 },
  "golden section counts",
);
assert(M.HEAPU16 instanceof Uint16Array);
for (const { wasmName } of BINDINGS) assert.equal(typeof M[wasmName], "function", `built module export ${wasmName}`);

// WASM uses musl's libm while the native goldens use the host libm. Solver moments can
// differ by up to the existing sub-millisecond cap; direct lunar values need only 1e-9.
const MAX_MOMENT_DIFF_DAYS = 1e-8;
const MAX_LUNAR_VALUE_DIFF = 1e-9;
// Native arm64 may contract one sidereal polynomial product into FMA; 1e-6 degree covers
// the resulting product ULP throughout the declared year window.
const MAX_SIDEREAL_DIFF_DEG = 1e-6;

const initialBuffer = M.HEAPU8.buffer;
const initialBytes = M.HEAPU8.byteLength;
const growthPtr = M._malloc(initialBytes);
assert.notEqual(growthPtr, 0, "growth allocation");
try {
  assert.notStrictEqual(M.HEAPU8.buffer, initialBuffer, "WASM memory must grow and refresh Module.HEAP* views");
  assert(M.HEAPU8.byteLength > initialBytes, "WASM memory grew");
} finally {
  M._free(growthPtr);
}

const decoder = new TextDecoder();
const happyExports = new Set();
const seenLayouts = new Set();
const layoutByExport = new Map(
  manifest.exports
    .filter(({ return: value }) => value.kind === "sret")
    .map(({ name, return: value }) => [name, value.layout]),
);

// Every read obtains the current Module.HEAP* view after the native call. Memory growth
// detaches old views, so no view may be retained by a binding helper.
const scalarLayouts = {
  bool: { size: 1, alignment: 1, read: "getUint8" },
  uint8_t: { size: 1, alignment: 1, read: "getUint8" },
  uint16_t: { size: 2, alignment: 2, read: "getUint16" },
  int32_t: { size: 4, alignment: 4, read: "getInt32" },
  uint32_t: { size: 4, alignment: 4, read: "getUint32" },
  double: { size: 8, alignment: 8, read: "getFloat64" },
};
const typeInfo = (type) => {
  if (Object.hasOwn(scalarLayouts, type)) return scalarLayouts[type];
  if (Object.hasOwn(manifest.layouts, type)) return { ...manifest.layouts[type], record: type };

  const array = type.match(/^([A-Za-z_]\w*)\[([1-9][0-9]*)\]$/);
  assert(array, `unknown field type ${type}`);
  const element = typeInfo(array[1]);
  const count = Number(array[2]);
  const size = element.size * count;
  assert(Number.isSafeInteger(count) && Number.isSafeInteger(size), `unsafe array type ${type}`);
  return { size, alignment: element.alignment, element: array[1], stride: element.size, count };
};
const checkedRange = (ptr, size, alignment) => {
  assert(Number.isSafeInteger(ptr) && ptr >= 0, `invalid heap address ${ptr}`);
  assert(Number.isSafeInteger(size) && size > 0, `invalid heap size ${size}`);
  assert(Number.isSafeInteger(alignment) && alignment > 0 && ptr % alignment === 0, `unaligned heap address ${ptr}`);
  assert(Number.isSafeInteger(ptr + size) && ptr + size <= M.HEAPU8.byteLength, `heap range ${ptr}+${size}`);
};
const readValue = (ptr, type) => {
  const info = typeInfo(type);
  checkedRange(ptr, info.size, info.alignment);
  if (info.element) {
    return Array.from({ length: info.count }, (_, index) => readValue(ptr + index * info.stride, info.element));
  }
  if (info.record) return readLayout(ptr, info.record);

  const value = new DataView(M.HEAPU8.buffer)[info.read](ptr, true);
  return type === "bool" ? value !== 0 : value;
};
const readLayout = (ptr, name) => {
  const layout = manifest.layouts[name];
  assert(layout, `unknown layout ${name}`);
  checkedRange(ptr, layout.size, layout.alignment);
  seenLayouts.add(name);
  return Object.fromEntries(layout.fields.map((field) => {
    const info = typeInfo(field.type);
    assert(Number.isSafeInteger(field.offset) && field.offset >= 0 && field.offset + info.size <= layout.size,
      `field outside record ${name}.${field.name}`);
    return [field.name, readValue(ptr + field.offset, field.type)];
  }));
};
const rawSret = (name, args) => {
  const layoutName = layoutByExport.get(name);
  const layout = manifest.layouts[layoutName];
  assert(layout, `unknown sret export ${name}`);
  assert.equal(typeof M[`_${name}`], "function", `built module export _${name}`);
  const ptr = M._malloc(layout.size + 8);
  assert.notEqual(ptr, 0, `${name} sret allocation`);
  try {
    checkedRange(ptr, layout.size + 8, layout.alignment);
    M.HEAPU8.fill(0xA5, ptr + layout.size, ptr + layout.size + 8);
    M[`_${name}`](ptr, ...args);
    assert(M.HEAPU8.subarray(ptr + layout.size, ptr + layout.size + 8).every((byte) => byte === 0xA5),
      `${name} wrote beyond its sret layout`);
    const valid = layout.fields.find((field) => field.name === "valid");
    assert(valid && valid.type === "bool", `${name} valid marker`);
    if (!readValue(ptr + valid.offset, "bool")) return { valid: false };
    return readLayout(ptr, layoutName);
  } finally {
    M._free(ptr);
  }
};
const validSret = (name, args) => {
  const value = rawSret(name, args);
  assert.equal(value.valid, true, `${name} valid`);
  happyExports.add(name);
  return value;
};
const readCString = (ptr) => {
  assert(ptr > 0, "borrowed string pointer");
  checkedRange(ptr, 1, 1);
  let end = ptr;
  while (end < M.HEAPU8.byteLength && M.HEAPU8[end] !== 0) ++end;
  assert(end < M.HEAPU8.byteLength, "unterminated C string");
  return decoder.decode(M.HEAPU8.slice(ptr, end));
};
const lastError = () => readCString(M._last_error());
const finite = (...values) => values.forEach((value) => assert(Number.isFinite(value)));
const readDoubles = (ptr, count) => Array.from({ length: count }, (_, index) => M.HEAPF64[(ptr >> 3) + index]);

assert.equal(M._set_log_verbosity(0), 1);
happyExports.add("set_log_verbosity");

const jd = validSret("ut1_to_jd", [2000, 1, 1, 0.5]);
assert.equal(jd.value, 2451545.0);
assert.equal(lastError(), "");
happyExports.add("last_error");

const jde = validSret("ut1_to_jde", [2024, 6, 1, 0.5]);
assert(jde.value > 2460463.0);
const ut1 = validSret("jde_to_ut1", [2460463.0]);
assert.deepEqual([ut1.year, ut1.month, ut1.day], [2024, 6, 1]);
finite(ut1.fraction);

const sun = validSret("sun_apparent_geocentric_coord", [2460463.0]);
assert(sun.lon >= 0 && sun.lon < 360);
finite(sun.lat, sun.r);
const moon = validSret("moon_apparent_geocentric_coord", [2460463.0]);
assert(moon.lon >= 0 && moon.lon < 360);
finite(moon.lat, moon.r);
const illumination = validSret("moon_illumination", [2448724.5]);
assert(Math.abs(illumination.illumination - 0.6786) < 5e-5);
assert(illumination.elongation_deg >= 0 && illumination.elongation_deg < 360);
const positionAngle = validSret("moon_position_angle", [2448724.5]);
assert(Math.abs(positionAngle.angle_deg - 285.0) < 0.05);

const countPtr = M._malloc(4);
const oneSlot = M._malloc(8);
try {
  M.HEAPU32[countPtr >> 2] = 0xDEADBEEF;
  assert.equal(M._moon_phase_moments(2024, 0, countPtr, 0, 0), 0);
  const phaseCount = M.HEAPU32[countPtr >> 2];
  assert(phaseCount === 12 || phaseCount === 13);
  assert.equal(M._moon_phase_moments(2024, 0, countPtr, oneSlot, 1), 1);
  assert.equal(M.HEAPU32[countPtr >> 2], phaseCount);
  finite(M.HEAPF64[oneSlot >> 3]);
  happyExports.add("moon_phase_moments");

  const discriminant = validSret("solar_lon_root_discriminant", [2024, 0.0]);
  assert.equal(discriminant.count, 1);
  assert.equal(M._solar_lon_roots(2024, 0.0, 0, 0), 0);
  assert.equal(lastError(), "");
  assert.equal(M._solar_lon_roots(2024, 0.0, oneSlot, 1), 1);
  finite(M.HEAPF64[oneSlot >> 3]);
  const noRoot = validSret("solar_lon_root_discriminant", [1, 281.3]);
  assert.equal(noRoot.count, 0, "valid discriminant can report no root");
  assert.equal(M._solar_lon_roots(1, 281.3, 0, 0), 0);
  assert.equal(lastError(), "");
  const twoRoots = validSret("solar_lon_root_discriminant", [2024, 280.1]);
  assert.equal(twoRoots.count, 2);
  const twoSlots = M._malloc(twoRoots.count * 8);
  try {
    assert.equal(M._solar_lon_roots(2024, 280.1, twoSlots, twoRoots.count), twoRoots.count);
    const roots = readDoubles(twoSlots, twoRoots.count);
    assert(roots[0] < roots[1]);
  } finally {
    M._free(twoSlots);
  }
  happyExports.add("solar_lon_roots");

  assert.equal(M._new_moons_after_jde(2460463.0, 0, 0), 0);
  assert.equal(lastError(), "");
  const requestedSlots = M._malloc(3 * 8);
  try {
    assert.equal(M._new_moons_after_jde(2460463.0, requestedSlots, 3), 3);
    const requested = readDoubles(requestedSlots, 3);
    assert(requested[0] < requested[1] && requested[1] < requested[2]);
  } finally {
    M._free(requestedSlots);
  }
  happyExports.add("new_moons_after_jde");

  M.HEAPU32[countPtr >> 2] = 0xDEADBEEF;
  assert.equal(M._new_moons_in_year(2024, countPtr, 0, 0), 0);
  const newMoonCount = M.HEAPU32[countPtr >> 2];
  assert(newMoonCount === 12 || newMoonCount === 13);
  assert.equal(M._new_moons_in_year(2024, countPtr, oneSlot, 1), 1);
  assert.equal(M.HEAPU32[countPtr >> 2], newMoonCount);
  finite(M.HEAPF64[oneSlot >> 3]);
  happyExports.add("new_moons_in_year");
} finally {
  M._free(oneSlot);
  M._free(countPtr);
}

const equation = validSret("equation_of_time", [2460463.0]);
assert(Math.abs(equation.value) < 5);
const apparent = validSret("apparent_solar_time", [2024, 6, 1, 0.5, 116.4]);
assert.deepEqual([apparent.year, apparent.month, apparent.day], [2024, 6, 1]);
assert(apparent.fraction > 0 && apparent.fraction < 1);
const sidereal = validSret("local_apparent_sidereal_time", [2460463.0, 120.0]);
assert(sidereal.value >= 0 && sidereal.value < 360);

const jieqi = validSret("query_jieqi_moment", [2024, 0]);
assert.deepEqual([jieqi.jq_idx, jieqi.y, jieqi.m], [0, 2024, 2]);
finite(jieqi.frac);
const JIEQI_NAMES = [
  "立春", "雨水", "惊蛰", "春分", "清明", "谷雨", "立夏", "小满", "芒种", "夏至", "小暑", "大暑",
  "立秋", "处暑", "白露", "秋分", "寒露", "霜降", "立冬", "小雪", "大雪", "冬至", "小寒", "大寒",
];
const namePtr = M._malloc(16);
try {
  for (const [index, expected] of JIEQI_NAMES.entries()) {
    assert.equal(M._get_jieqi_name(index, namePtr, 16), 1);
    assert.equal(readCString(namePtr), expected);
  }
  happyExports.add("get_jieqi_name");
} finally {
  M._free(namePtr);
}

const lunarRange = validSret("get_supported_lunar_year_range", [3]);
assert.deepEqual([lunarRange.start, lunarRange.end], [1600, 2199]);
const lunarInfo = validSret("get_lunar_year_info", [2, 2024]);
assert.deepEqual([lunarInfo.year, lunarInfo.month, lunarInfo.day], [2024, 2, 10]);
assert.equal(lunarInfo.leap_month, 0);
assert(lunarInfo.month_len > 0);
const lunarDate = validSret("gregorian_to_lunar", [1, 2023, 3, 22]);
assert.deepEqual(
  [lunarDate.year, lunarDate.month, lunarDate.is_leap, lunarDate.day],
  [2023, 2, true, 1],
);
const gregorianDate = validSret("lunar_to_gregorian", [1, 2023, 2, true, 1]);
assert.deepEqual([gregorianDate.year, gregorianDate.month, gregorianDate.day], [2023, 3, 22]);

for (const name of ["delta_t_algo1", "delta_t_algo2", "delta_t_algo3", "delta_t_algo4", "delta_t_algo5", "delta_t"]) {
  finite(validSret(name, [2024.5]).value);
}

const recordedNames = new Set();
const recordedFailure = (name, invoke) => {
  assert.equal(rawSret("ut1_to_jd", [2000, 1, 1, 0.5]).valid, true, "recording precondition");
  assert.equal(lastError(), "", `${name} starts with empty last_error`);
  assert.equal(invoke(), true, `${name} failure result`);
  assert(lastError().length > 0, `${name} records last_error`);
  recordedNames.add(name);
};
recordedFailure("ut1_to_jd", () => !rawSret("ut1_to_jd", [2024, 6, 1, 1.5]).valid);
recordedFailure("ut1_to_jde", () => !rawSret("ut1_to_jde", [2024, 13, 1, 0.5]).valid);
recordedFailure("jde_to_ut1", () => !rawSret("jde_to_ut1", [Number.NaN]).valid);
recordedFailure("moon_illumination", () => !rawSret("moon_illumination", [Number.NaN]).valid);
recordedFailure("moon_position_angle", () => !rawSret("moon_position_angle", [Number.NaN]).valid);
recordedFailure("moon_phase_moments", () => {
  const rootCount = M._malloc(4);
  try {
    M.HEAPU32[rootCount >> 2] = 0xDEADBEEF;
    const written = M._moon_phase_moments(2024, 4, rootCount, 0, 0);
    return written === 0 && M.HEAPU32[rootCount >> 2] === 0;
  } finally {
    M._free(rootCount);
  }
});
recordedFailure("local_apparent_sidereal_time", () => !rawSret("local_apparent_sidereal_time", [1000000.0, 0]).valid);

assert.equal(M._moon_phase_moments(2024, 0, 0, 0, 0), 0);
assert(lastError().length > 0, "moon_phase_moments rejects null root_count");
const nullProtocolCount = M._malloc(4);
try {
  M.HEAPU32[nullProtocolCount >> 2] = 0xDEADBEEF;
  assert.equal(M._moon_phase_moments(2024, 0, nullProtocolCount, 0, 1), 0);
  assert.equal(M.HEAPU32[nullProtocolCount >> 2], 0);
  assert(lastError().length > 0, "moon_phase_moments rejects null slots for a positive count");

  assert.equal(M._new_moons_in_year(2024, 0, 0, 0), 0);
  M.HEAPU32[nullProtocolCount >> 2] = 0xDEADBEEF;
  assert.equal(M._new_moons_in_year(2024, nullProtocolCount, 0, 1), 0);
  assert.equal(M.HEAPU32[nullProtocolCount >> 2], 0);
  assert.equal(M._solar_lon_roots(2024, 0.0, 0, 1), 0);
  assert.equal(M._new_moons_after_jde(2460463.0, 0, 1), 0);
} finally {
  M._free(nullProtocolCount);
}

const stale = lastError();
assert(stale.length > 0);
assert.equal(rawSret("sun_apparent_geocentric_coord", [Number.NaN]).valid, false);
assert(lastError().length > 0 && lastError() !== stale, "new recording failure replaces last_error");
assert.equal(rawSret("moon_illumination", [2448724.5]).valid, true);
assert.equal(lastError(), "", "successful recording call clears last_error");

recordedFailure("set_log_verbosity", () => M._set_log_verbosity(3) === 0);
recordedFailure("sun_apparent_geocentric_coord", () => !rawSret("sun_apparent_geocentric_coord", [Number.NaN]).valid);
recordedFailure("moon_apparent_geocentric_coord", () => !rawSret("moon_apparent_geocentric_coord", [Number.NaN]).valid);
recordedFailure("solar_lon_root_discriminant", () => !rawSret("solar_lon_root_discriminant", [2024, Number.NaN]).valid);
assert.equal(rawSret("solar_lon_root_discriminant", [0, 0]).valid, false);
const edgeSlot = M._malloc(8);
const edgeCount = M._malloc(4);
try {
  recordedFailure("solar_lon_roots", () => M._solar_lon_roots(2024, Number.NaN, edgeSlot, 1) === 0);
  recordedFailure("new_moons_after_jde", () => M._new_moons_after_jde(Number.NaN, edgeSlot, 1) === 0);
  M.HEAPU32[edgeCount >> 2] = 0xDEADBEEF;
  recordedFailure("new_moons_in_year", () => M._new_moons_in_year(0, edgeCount, edgeSlot, 1) === 0);
  assert.equal(M.HEAPU32[edgeCount >> 2], 0);
} finally {
  M._free(edgeCount);
  M._free(edgeSlot);
}
recordedFailure("equation_of_time", () => !rawSret("equation_of_time", [Number.NaN]).valid);
recordedFailure("apparent_solar_time", () => !rawSret("apparent_solar_time", [2024, 6, 1, 0.5, 200]).valid);
recordedFailure("query_jieqi_moment", () => !rawSret("query_jieqi_moment", [2024, 24]).valid);
recordedFailure("get_supported_lunar_year_range", () => !rawSret("get_supported_lunar_year_range", [0]).valid);
recordedFailure("get_lunar_year_info", () => !rawSret("get_lunar_year_info", [9, 2024]).valid);
recordedFailure("gregorian_to_lunar", () => !rawSret("gregorian_to_lunar", [1, 2023, 13, 1]).valid);
recordedFailure("lunar_to_gregorian", () => !rawSret("lunar_to_gregorian", [1, 2024, 2, true, 1]).valid);
for (const name of ["delta_t_algo1", "delta_t_algo2", "delta_t_algo3", "delta_t_algo4", "delta_t_algo5", "delta_t"]) {
  recordedFailure(name, () => !rawSret(name, [Number.NaN]).valid);
}

const tinyName = M._malloc(2);
try {
  M.HEAPU8[tinyName] = 0x41;
  M.HEAPU8[tinyName + 1] = 0x42;
  recordedFailure("get_jieqi_name", () => M._get_jieqi_name(0, tinyName, 2) === 0);
  assert.deepEqual([M.HEAPU8[tinyName], M.HEAPU8[tinyName + 1]], [0x41, 0x42]);
  assert.equal(M._get_jieqi_name(0, 0, 16), 0);
  assert.equal(M._get_jieqi_name(24, tinyName, 2), 0);
} finally {
  M._free(tinyName);
}

assert.equal(rawSret("query_jieqi_moment", [40000, 13]).valid, false);
assert.equal(validSret("query_jieqi_moment", [2026, 13]).y, 2026, "module survives a translated C++ exception");

const f64 = new Float64Array(1);
const u64 = new BigUint64Array(f64.buffer);
const bitsOf = (hex) => {
  u64[0] = BigInt(hex);
  return f64[0];
};
const close = (actual, expected, tolerance, label) => {
  const difference = Math.abs(actual - expected);
  assert(Number.isFinite(difference) && difference <= tolerance, `${label}: ${difference} > ${tolerance}`);
};

const chartArgs = (changes = {}) => {
  const input = {
    year: 2026, month: 1, day: 1, fraction: 0.5, civil_scale_code: 1,
    latitude_deg: 40, longitude_deg: -75, house_system_code: 0, delta_t_model_code: 0,
    ...changes,
  };
  return [
    input.year, input.month, input.day, input.fraction, input.civil_scale_code,
    input.latitude_deg, input.longitude_deg, input.house_system_code, input.delta_t_model_code,
  ];
};
const validChart = (changes = {}) => {
  const chart = validSret("chart_snapshot_v1", chartArgs(changes));
  assert.equal(lastError(), "", "successful chart clears last_error");
  finite(chart.jd_ut1, chart.jde_tt);
  assert.equal(chart.bodies.length, 14);
  assert.deepEqual(chart.bodies.map(({ target_code }) => target_code), Array.from({ length: 14 }, (_, index) => index));
  for (const [index, body] of chart.bodies.entries()) {
    finite(body.longitude_deg, body.latitude_deg, body.longitude_rate_deg_per_tt_day);
    assert(body.longitude_deg >= 0 && body.longitude_deg < 360);
    assert.equal(body.present_fields, index < 10 ? 3 : 1, `chart presence bits ${index}`);
    if (index < 10) {
      finite(body.distance_au);
      assert(body.distance_au > 0, `chart distance in AU ${index}`);
    } else {
      assert.equal(body.latitude_deg, 0, `node latitude is present zero ${index}`);
      // Node distance storage has no contract; only the presence bit is consumed.
    }
  }
  for (const ascending of [10, 12]) {
    const a = chart.bodies[ascending];
    const d = chart.bodies[ascending + 1];
    close((d.longitude_deg - a.longitude_deg + 360) % 360, 180, 1e-10, "node antipodes");
    assert.equal(a.longitude_rate_deg_per_tt_day, d.longitude_rate_deg_per_tt_day, "paired node rates");
  }
  assert.equal(chart.houses.cusps_deg.length, 12);
  for (const value of [
    chart.houses.ascendant_deg, chart.houses.midheaven_deg,
    chart.houses.descendant_deg, chart.houses.imum_coeli_deg, ...chart.houses.cusps_deg,
  ]) {
    finite(value);
    assert(value >= 0 && value < 360, "chart house angle range");
  }
  return chart;
};
const usedChartPayload = (chart) => ({
  ...chart,
  bodies: chart.bodies.map(({ distance_au, ...body }) =>
    body.present_fields & 2 ? { ...body, distance_au } : body),
});

const chart = validChart();
assert(chart.jde_tt > chart.jd_ut1, "modern chart has positive Delta T");
assert.equal(chart.jd_ut1, validSret("ut1_to_jd", [2026, 1, 1, 0.5]).value);
assert.equal(chart.jde_tt, validSret("ut1_to_jde", [2026, 1, 1, 0.5]).value);
const explicitAlgo5 = validChart({ delta_t_model_code: 5 });
assert.deepEqual(usedChartPayload(chart), usedChartPayload(explicitAlgo5), "chart DEFAULT is exactly Algo5");
const explicitAlgo1 = validChart({ delta_t_model_code: 1 });
assert.notEqual(chart.jde_tt, explicitAlgo1.jde_tt, "DEFAULT must not enum-cast to Algo1");
assert.notEqual(chart.bodies[0].longitude_deg, explicitAlgo1.bodies[0].longitude_deg);

for (const model of [1, 2, 3, 4, 5]) {
  const selected = validChart({ delta_t_model_code: model });
  const delta = validSret(`delta_t_algo${model}`, [2026 + 0.5 / 365]).value;
  const expectedTt = validSret("ut1_to_jd", [2026, 1, 1, 0.5 + delta / 86400]).value;
  assert.equal(selected.jd_ut1, chart.jd_ut1, `UT1 ingress unchanged for model ${model}`);
  assert.equal(selected.jde_tt, expectedTt, `chart model ${model} uses the selected Delta T export`);
}

const utcChart = validChart({ civil_scale_code: 0 });
const utcAlgo1 = validChart({ civil_scale_code: 0, delta_t_model_code: 1 });
// #300's modern UTC replay: frozen Delta AT 37 s plus TT-TAI 32.184 s at this date.
assert.equal(utcChart.jde_tt, validSret("ut1_to_jd", [2026, 1, 1, 0.5 + 69.184 / 86400]).value);
assert.equal(utcChart.jde_tt, utcAlgo1.jde_tt, "UTC to TT is independent of Delta T model");
assert.notEqual(utcChart.jd_ut1, utcAlgo1.jd_ut1, "UTC to modelled UT1 uses the selected model");
assert.notEqual(utcChart.jd_ut1, chart.jd_ut1, "UTC is not silently interpreted as UT1");

const chartSun = validSret("sun_apparent_geocentric_coord", [chart.jde_tt]);
const chartMoon = validSret("moon_apparent_geocentric_coord", [chart.jde_tt]);
assert.deepEqual(
  [chart.bodies[0].longitude_deg, chart.bodies[0].latitude_deg, chart.bodies[0].distance_au],
  [chartSun.lon, chartSun.lat, chartSun.r],
  "chart Sun keeps the existing degree/AU units",
);
assert.deepEqual([chart.bodies[1].longitude_deg, chart.bodies[1].latitude_deg], [chartMoon.lon, chartMoon.lat]);
// The exact AU definition in toolbox.hpp; the old Moon export remains in kilometres.
assert.equal(chartMoon.r, chart.bodies[1].distance_au * 149597870.700);

for (const system of [0, 1, 2]) {
  const selected = validChart({ house_system_code: system });
  assert.deepEqual(usedChartPayload(selected).bodies, usedChartPayload(chart).bodies, "houses do not affect ephemerides");
  const { cusps_deg: cusps, ...axes } = selected.houses;
  const { cusps_deg: equalCusps, ...equalAxes } = chart.houses;
  assert.deepEqual(axes, equalAxes, "all house systems keep the same axes");
  if (system < 2) {
    const first = system === 0 ? axes.ascendant_deg : Math.floor(axes.ascendant_deg / 30) * 30;
    for (const [index, cusp] of cusps.entries()) {
      close(cusp, (first + index * 30) % 360, 1e-10, `house system ${system} cusp ${index + 1}`);
    }
    if (system === 1) assert.notDeepEqual(cusps, equalCusps, "Whole Sign is not Equal fallback");
  } else {
    assert.deepEqual([cusps[0], cusps[3], cusps[6], cusps[9]],
      [axes.ascendant_deg, axes.imum_coeli_deg, axes.descendant_deg, axes.midheaven_deg]);
    assert.notDeepEqual(cusps, equalCusps, "Placidus is not Equal fallback");
  }
}

const southern = validChart({ latitude_deg: -33.87, longitude_deg: 151.21, house_system_code: 2 });
assert.notDeepEqual(southern.houses, chart.houses);
assert.deepEqual(usedChartPayload(southern).bodies, usedChartPayload(chart).bodies, "location affects houses only");

const west = validChart({ latitude_deg: 0, longitude_deg: -1 });
const east = validChart({ latitude_deg: 0, longitude_deg: 1 });
const eastwardMidheaven = ((east.houses.midheaven_deg - west.houses.midheaven_deg + 540) % 360) - 180;
assert(eastwardMidheaven > 0, "east-positive geographic longitude advances the Midheaven");

const retrograde = validChart({ year: 2025, month: 3, day: 25, latitude_deg: -33.87, longitude_deg: 151.21, house_system_code: 2 });
assert(retrograde.bodies[2].longitude_rate_deg_per_tt_day < 0, "signed Mercury retrograde rate survives the ABI");
assert(retrograde.bodies[10].longitude_rate_deg_per_tt_day < 0, "signed mean-node rate survives the ABI");

const historical = validChart({ year: 1900, month: 6, day: 1 });
assert.equal(historical.jd_ut1, validSret("ut1_to_jd", [1900, 6, 1, 0.5]).value);

validChart({ year: 2000, month: 2, day: 29 });
validChart({ year: 1972, fraction: 0, civil_scale_code: 0 });
for (const longitude_deg of [-180, 180]) validChart({ latitude_deg: 0, longitude_deg });

let chartFailureCount = 0;
const invalidChart = (changes, expectedError) => {
  recordedFailure("chart_snapshot_v1", () => !rawSret("chart_snapshot_v1", chartArgs(changes)).valid);
  const error = lastError();
  if (expectedError) assert(error.includes(expectedError), `chart error for ${JSON.stringify(changes)}: ${error}`);
  ++chartFailureCount;
};
for (const civil_scale_code of [2, 255, 256, 257, 0x10000001, 0xFFFFFFFF]) {
  invalidChart({ civil_scale_code }, "civil_scale_code");
}
for (const house_system_code of [3, 255, 256, 257, 0x10000002, 0xFFFFFFFF]) {
  invalidChart({ house_system_code }, "house_system_code");
}
for (const delta_t_model_code of [6, 255, 256, 257, 261, 0x10000005, 0xFFFFFFFF]) {
  invalidChart({ delta_t_model_code }, "delta_t_model_code");
}
for (const year of [-2147483648, -63510, 1884, 2100, 67562, 2147483647]) invalidChart({ year }, "year");
for (const month of [0, 13, 256, 257, 268, 65537, 0xFFFFFFFF]) invalidChart({ month }, "month");
for (const day of [0, 32, 256, 257, 287, 65537, 0xFFFFFFFF]) invalidChart({ day }, "day");
invalidChart({ year: 1900, month: 2, day: 29 }, "day");
invalidChart({ month: 4, day: 31 }, "day");
for (const fraction of [NaN, Infinity, -Infinity, -0.1, 1, 1e100]) invalidChart({ fraction }, "fraction");
for (const latitude_deg of [NaN, Infinity, -Infinity, -90, 90]) invalidChart({ latitude_deg }, "location.");
for (const longitude_deg of [NaN, Infinity, -Infinity, -181, 181]) invalidChart({ longitude_deg }, "location.");
invalidChart({ year: 1971, civil_scale_code: 0 }, "1972");

invalidChart({ year: 1885, fraction: 0, civil_scale_code: 1 }, "Chart JDE(TT)");
validChart({ year: 1885, day: 3, fraction: 0, civil_scale_code: 1 });
invalidChart({ year: 2099, month: 12, day: 31, fraction: 0.999999, civil_scale_code: 1 }, "Chart JDE(TT)");
validChart({ year: 2099, month: 12, day: 31, fraction: 0.5, civil_scale_code: 1 });

invalidChart({ year: 2035, month: 6, delta_t_model_code: 4 });
validChart({ year: 2035, month: 6 });
for (const latitude_deg of [-70, 70]) {
  invalidChart({ latitude_deg, house_system_code: 2 }, "Placidus");
  validChart({ latitude_deg, house_system_code: 0 });
  validChart({ latitude_deg, house_system_code: 1 });
}
assert.equal(rawSret("chart_snapshot_v1", chartArgs({ fraction: NaN })).valid, false);
const chartError = lastError();
assert(chartError.includes("fraction"));
assert.equal(rawSret("chart_snapshot_v1", chartArgs({ civil_scale_code: 257 })).valid, false);
assert(lastError().includes("civil_scale_code") && lastError() !== chartError, "chart failure replaces last_error");
assert.equal(lastError(), lastError(), "last_error itself preserves the recorded chart failure");
validChart();

let goldenCount = 0;
for (const point of golden.sections.jieqi.entries) {
  const value = validSret("query_jieqi_moment", [point.year, point.idx]);
  assert.deepEqual([value.jq_idx, value.y, value.m, value.d], [point.idx, point.y, point.m, point.d]);
  close(value.frac, bitsOf(point.frac_bits), MAX_MOMENT_DIFF_DAYS, "jieqi frac");
  ++goldenCount;
}
for (const point of golden.sections.moon.entries) {
  const value = validSret("moon_illumination", [bitsOf(point.jde_bits)]);
  close(value.illumination, bitsOf(point.illumination_bits), MAX_LUNAR_VALUE_DIFF, "moon illumination");
  close(value.elongation_deg, bitsOf(point.elongation_deg_bits), MAX_LUNAR_VALUE_DIFF, "moon elongation");
  ++goldenCount;
}
for (const point of golden.sections.sidereal.entries) {
  const value = validSret("local_apparent_sidereal_time", [bitsOf(point.jd_ut1_bits), point.longitude]);
  close(value.value, bitsOf(point.value_bits), MAX_SIDEREAL_DIFF_DEG, "sidereal");
  ++goldenCount;
}
for (const point of golden.sections.moon_position_angle.entries) {
  const value = validSret("moon_position_angle", [bitsOf(point.jde_bits)]);
  close(value.angle_deg, bitsOf(point.angle_deg_bits), MAX_LUNAR_VALUE_DIFF, "moon position angle");
  ++goldenCount;
}
const phaseCache = new Map();
const phaseMoments = (year, phaseKind) => {
  const rootCount = M._malloc(4);
  try {
    assert.equal(M._moon_phase_moments(year, phaseKind, rootCount, 0, 0), 0);
    const count = M.HEAPU32[rootCount >> 2];
    assert(count > 0);
    const slots = M._malloc(count * 8);
    try {
      assert.equal(M._moon_phase_moments(year, phaseKind, rootCount, slots, count), count);
      assert.equal(M.HEAPU32[rootCount >> 2], count);
      return readDoubles(slots, count);
    } finally {
      M._free(slots);
    }
  } finally {
    M._free(rootCount);
  }
};
for (const point of golden.sections.phases.entries) {
  const key = `${point.year}:${point.phase_kind}`;
  if (!phaseCache.has(key)) phaseCache.set(key, phaseMoments(point.year, point.phase_kind));
  close(phaseCache.get(key)[point.index], bitsOf(point.jde_bits), MAX_MOMENT_DIFF_DAYS, "moon phase moment");
  ++goldenCount;
}

const expectedExports = BINDINGS.map(({ cName }) => cName);
assert.deepEqual(
  [...recordedNames].sort(),
  BINDINGS.filter(({ readsLastError }) => readsLastError).map(({ cName }) => cName).sort(),
  "all recording exports exercised a failure",
);
assert.equal(goldenCount, 389, "golden replay count");
assert.deepEqual([...happyExports].sort(), expectedExports.sort(), "all 30 exports executed successfully");
assert.deepEqual([...seenLayouts].sort(), Object.keys(manifest.layouts).sort(), "all 19 layouts decoded");
assert.deepEqual(
  [...new Set(manifest.exports.map(({ protocol }) => protocol.kind).filter((kind) => kind.endsWith("fill")))].sort(),
  ["companion-fill", "count-fill", "requested-fill"],
  "three count/fill protocol classes",
);

console.log("PASS raw exports 30/30; layouts 19/19; recording seams 29/29");
console.log(`PASS private chart sret: order, absence, units, systems, scales, DEFAULT/models; rejected inputs ${chartFailureCount}`);
console.log("PASS caller string + borrowed string + three count/fill classes + legal zero");
console.log("PASS memory growth refreshed HEAP views; translated exception survived");
console.log(`PASS shared binding golden replay ${goldenCount}/389`);
