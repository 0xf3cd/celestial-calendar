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
import { readFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

import { BINDINGS } from "../../src/bindings.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const REPO = resolve(HERE, "../../../..");
const manifest = JSON.parse(await readFile(resolve(HERE, "manifest.json"), "utf8"));
const golden = JSON.parse(await readFile(resolve(REPO, "toolbox/wasm_golden.json"), "utf8"));
const M = await (await import(pathToFileURL(resolve(REPO, "build/wasm/celestial-jieqi.mjs")))).default();

assert.equal(golden.schema, "celestial-calendar/wasm-golden@2");
assert.deepEqual(
  Object.fromEntries(Object.entries(golden.sections).map(([name, section]) => [name, section.entries.length])),
  { jieqi: 204, moon: 41, sidereal: 43, moon_position_angle: 41, phases: 60 },
  "golden section counts",
);
assert(M.HEAPU16 instanceof Uint16Array);

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
assert.notStrictEqual(M.HEAPU8.buffer, initialBuffer, "WASM memory must grow and refresh Module.HEAP* views");
assert(M.HEAPU8.byteLength > initialBytes, "WASM memory grew");
M._free(growthPtr);

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
const readField = (ptr, field) => {
  const address = ptr + field.offset;
  switch (field.type) {
    case "bool": return M.HEAPU8[address] !== 0;
    case "uint8_t": return M.HEAPU8[address];
    case "uint16_t": return M.HEAPU16[address >> 1];
    case "int32_t": return M.HEAP32[address >> 2];
    case "uint32_t": return M.HEAPU32[address >> 2];
    case "double": return M.HEAPF64[address >> 3];
    default: throw new Error(`unknown field type ${field.type}`);
  }
};
const readLayout = (ptr, name) => {
  const layout = manifest.layouts[name];
  assert(layout, `unknown layout ${name}`);
  seenLayouts.add(name);
  return Object.fromEntries(layout.fields.map((field) => [field.name, readField(ptr, field)]));
};
const rawSret = (name, args) => {
  const layoutName = layoutByExport.get(name);
  const ptr = M._malloc(manifest.layouts[layoutName].size);
  try {
    M[`_${name}`](ptr, ...args);
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
  let end = ptr;
  while (M.HEAPU8[end] !== 0) ++end;
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
  assert.equal(M._solar_lon_roots(2024, 0.0, oneSlot, 1), 1);
  finite(M.HEAPF64[oneSlot >> 3]);
  const noRoot = validSret("solar_lon_root_discriminant", [1, 281.3]);
  assert.equal(noRoot.count, 0, "valid discriminant can report no root");
  assert.equal(M._solar_lon_roots(1, 281.3, 0, 0), 0);
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

const recordedFailure = (name, invoke) => {
  assert.equal(invoke(), true, `${name} failure result`);
  assert(lastError().length > 0, `${name} records last_error`);
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
assert.equal(lastError(), stale, "non-recording failure leaves stale last_error untouched");
assert.equal(rawSret("moon_illumination", [2448724.5]).valid, true);
assert.equal(lastError(), "", "successful recording call clears last_error");

assert.equal(M._set_log_verbosity(3), 0);
assert.equal(rawSret("moon_apparent_geocentric_coord", [Number.NaN]).valid, false);
assert.equal(rawSret("solar_lon_root_discriminant", [2024, Number.NaN]).valid, false);
assert.equal(rawSret("solar_lon_root_discriminant", [0, 0]).valid, false);
const edgeSlot = M._malloc(8);
const edgeCount = M._malloc(4);
try {
  assert.equal(M._solar_lon_roots(2024, Number.NaN, edgeSlot, 1), 0);
  assert.equal(M._new_moons_after_jde(Number.NaN, edgeSlot, 1), 0);
  M.HEAPU32[edgeCount >> 2] = 0xDEADBEEF;
  assert.equal(M._new_moons_in_year(0, edgeCount, edgeSlot, 1), 0);
  assert.equal(M.HEAPU32[edgeCount >> 2], 0);
} finally {
  M._free(edgeCount);
  M._free(edgeSlot);
}
assert.equal(rawSret("equation_of_time", [Number.NaN]).valid, false);
assert.equal(rawSret("apparent_solar_time", [2024, 6, 1, 0.5, 200]).valid, false);
assert.equal(rawSret("query_jieqi_moment", [2024, 24]).valid, false);
assert.equal(rawSret("get_supported_lunar_year_range", [0]).valid, false);
assert.equal(rawSret("get_lunar_year_info", [9, 2024]).valid, false);
assert.equal(rawSret("gregorian_to_lunar", [1, 2023, 13, 1]).valid, false);
assert.equal(rawSret("lunar_to_gregorian", [1, 2024, 2, true, 1]).valid, false);
for (const name of ["delta_t_algo1", "delta_t_algo2", "delta_t_algo3", "delta_t_algo4", "delta_t_algo5", "delta_t"]) {
  assert.equal(rawSret(name, [Number.NaN]).valid, false);
}

const tinyName = M._malloc(2);
try {
  M.HEAPU8[tinyName] = 0x41;
  M.HEAPU8[tinyName + 1] = 0x42;
  assert.equal(M._get_jieqi_name(0, tinyName, 2), 0);
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
assert.equal(goldenCount, 389, "golden replay count");
assert.deepEqual([...happyExports].sort(), expectedExports.sort(), "all 29 exports executed successfully");
assert.deepEqual([...seenLayouts].sort(), Object.keys(manifest.layouts).sort(), "all 16 layouts decoded");
assert.deepEqual(
  [...new Set(manifest.exports.map(({ protocol }) => protocol.kind).filter((kind) => kind.endsWith("fill")))].sort(),
  ["companion-fill", "count-fill", "requested-fill"],
  "three count/fill protocol classes",
);

console.log("PASS raw exports 29/29; layouts 16/16; recording seams 7/7");
console.log("PASS caller string + borrowed string + three count/fill classes + legal zero");
console.log("PASS memory growth refreshed HEAP views; translated exception survived");
console.log(`PASS existing WASM golden replay ${goldenCount}/389`);
