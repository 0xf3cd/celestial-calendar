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
import "./generated_view_test.mjs";

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
  {
    jieqi: 204, moon: 41, sidereal: 43, moon_position_angle: 41, phases: 60,
    longitude_events: 3, longitude_stations: 1, longitude_difference_events: 2,
  },
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
  assert.equal(M._celestial_last_error_kind(), 0, `${name} error kind`);
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
const lastError = () => readCString(M._celestial_last_error());
const finite = (...values) => values.forEach((value) => assert(Number.isFinite(value)));
const readDoubles = (ptr, count) => Array.from({ length: count }, (_, index) => M.HEAPF64[(ptr >> 3) + index]);

const jd = validSret("celestial_ut1_to_jd", [2000, 1, 1, 0.5]);
assert.equal(jd.value, 2451545.0);
assert.equal(lastError(), "");
happyExports.add("celestial_last_error");
assert.equal(M._celestial_last_error_kind(), 0);
happyExports.add("celestial_last_error_kind");

const jde = validSret("celestial_ut1_to_jde", [2024, 6, 1, 0.5]);
assert(jde.value > 2460463.0);
const ut1 = validSret("celestial_jde_to_ut1", [2460463.0]);
assert.deepEqual([ut1.year, ut1.month, ut1.day], [2024, 6, 1]);
finite(ut1.fraction);

const sun = validSret("celestial_sun_apparent_geocentric_coord", [2460463.0]);
assert(sun.lon >= 0 && sun.lon < 360);
finite(sun.lat, sun.r);
const moon = validSret("celestial_moon_apparent_geocentric_coord", [2460463.0]);
assert(moon.lon >= 0 && moon.lon < 360);
finite(moon.lat, moon.r);
const illumination = validSret("celestial_moon_illumination", [2448724.5]);
assert(Math.abs(illumination.illumination - 0.6786) < 5e-5);
assert(illumination.elongation_deg >= 0 && illumination.elongation_deg < 360);
const positionAngle = validSret("celestial_moon_position_angle", [2448724.5]);
assert(Math.abs(positionAngle.angle_deg - 285.0) < 0.05);

const countPtr = M._malloc(4);
const oneSlot = M._malloc(8);
try {
  M.HEAPU32[countPtr >> 2] = 0xDEADBEEF;
  assert.equal(M._celestial_moon_phase_moments(2024, 0, countPtr, 0, 0), 0);
  const phaseCount = M.HEAPU32[countPtr >> 2];
  assert(phaseCount === 12 || phaseCount === 13);
  assert.equal(M._celestial_moon_phase_moments(2024, 0, countPtr, oneSlot, 1), 1);
  assert.equal(M.HEAPU32[countPtr >> 2], phaseCount);
  finite(M.HEAPF64[oneSlot >> 3]);
  happyExports.add("celestial_moon_phase_moments");

  const discriminant = validSret("celestial_solar_lon_root_discriminant", [2024, 0.0]);
  assert.equal(discriminant.count, 1);
  assert.equal(M._celestial_solar_lon_roots(2024, 0.0, 0, 0), 0);
  assert.equal(lastError(), "");
  assert.equal(M._celestial_solar_lon_roots(2024, 0.0, oneSlot, 1), 1);
  finite(M.HEAPF64[oneSlot >> 3]);
  const noRoot = validSret("celestial_solar_lon_root_discriminant", [1, 281.3]);
  assert.equal(noRoot.count, 0, "valid discriminant can report no root");
  assert.equal(M._celestial_solar_lon_roots(1, 281.3, 0, 0), 0);
  assert.equal(lastError(), "");
  const twoRoots = validSret("celestial_solar_lon_root_discriminant", [2024, 280.1]);
  assert.equal(twoRoots.count, 2);
  const twoSlots = M._malloc(twoRoots.count * 8);
  try {
    assert.equal(M._celestial_solar_lon_roots(2024, 280.1, twoSlots, twoRoots.count), twoRoots.count);
    const roots = readDoubles(twoSlots, twoRoots.count);
    assert(roots[0] < roots[1]);
  } finally {
    M._free(twoSlots);
  }
  happyExports.add("celestial_solar_lon_roots");

  assert.equal(M._celestial_new_moons_after_jde(2460463.0, 0, 0), 0);
  assert.equal(lastError(), "");
  const requestedSlots = M._malloc(3 * 8);
  try {
    assert.equal(M._celestial_new_moons_after_jde(2460463.0, requestedSlots, 3), 3);
    const requested = readDoubles(requestedSlots, 3);
    assert(requested[0] < requested[1] && requested[1] < requested[2]);
  } finally {
    M._free(requestedSlots);
  }
  happyExports.add("celestial_new_moons_after_jde");

  M.HEAPU32[countPtr >> 2] = 0xDEADBEEF;
  assert.equal(M._celestial_new_moons_in_year(2024, countPtr, 0, 0), 0);
  const newMoonCount = M.HEAPU32[countPtr >> 2];
  assert(newMoonCount === 12 || newMoonCount === 13);
  assert.equal(M._celestial_new_moons_in_year(2024, countPtr, oneSlot, 1), 1);
  assert.equal(M.HEAPU32[countPtr >> 2], newMoonCount);
  finite(M.HEAPF64[oneSlot >> 3]);
  happyExports.add("celestial_new_moons_in_year");

  // Event count+fill: one count pass, one fill pass, records decoded from the manifest layout.
  const eventFill = (exportName, recordLayout, args) => {
    const recordSize = manifest.layouts[recordLayout].size;
    const fillCount = M._malloc(4);
    try {
      M.HEAPU32[countPtr >> 2] = 0xDEADBEEF;
      assert.equal(M[`_${exportName}`](...args, countPtr, 0, 0), 0);
      const count = M.HEAPU32[countPtr >> 2];
      assert(count > 0, `${exportName} fixture window must hold events`);
      const slots = M._malloc(count * recordSize);
      try {
        assert.equal(M[`_${exportName}`](...args, fillCount, slots, count), count);
        assert.equal(M.HEAPU32[fillCount >> 2], count);
        return Array.from({ length: count }, (_, index) => readLayout(slots + index * recordSize, recordLayout));
      } finally {
        M._free(slots);
      }
    } finally {
      M._free(fillCount);
    }
  };
  const mercuryCrossings = eventFill("celestial_longitude_events", "CelestialLongitudeEventRecord", [2, 0.0, 2460735.5, 2460785.5]);
  assert.equal(mercuryCrossings.length, 3);
  assert.deepEqual(mercuryCrossings.map((event) => event.direction_code), [0, 1, 0]);
  happyExports.add("celestial_longitude_events");
  const mercuryStations = eventFill("celestial_longitude_stations", "CelestialStationEventRecord", [2, 2460735.5, 2460785.5]);
  assert.equal(mercuryStations.length, 2);
  assert.deepEqual(
    mercuryStations.map((station) => [station.motion_before_code, station.motion_after_code]).sort(),
    [[0, 1], [1, 0]],
  );
  happyExports.add("celestial_longitude_stations");
  const moonSunSixty = eventFill(
    "celestial_longitude_difference_events", "CelestialLongitudeDifferenceEventRecord", [1, 0, -60.0, 2460676.5, 2460707.5],
  );
  assert.equal(moonSunSixty.length, 1);
  assert(Math.abs(moonSunSixty[0].signed_difference_deg + 60.0) < 1e-6);
  happyExports.add("celestial_longitude_difference_events");

  // Capability count+fill: the same protocol over the two capability record layouts.
  const eventCapabilities = eventFill("celestial_capabilities", "CelestialCapabilityRecord", [2, 0xFFFFFFFF]);
  assert.equal(eventCapabilities.length, 14);
  const plutoCapability = eventCapabilities.find((record) => record.subject_code === 9);
  assert.equal(plutoCapability.status_code, 1); // implementation window: center domain shrunk by the stencil reach
  assert.equal(plutoCapability.domain_start_jde_tt, 2409543.875);
  assert.equal(plutoCapability.maximum_span_tt_days, 3660);
  happyExports.add("celestial_capabilities");
  const identities = eventFill("celestial_model_identities", "CelestialModelIdentityRecord", [0xFFFFFFFF]);
  assert.equal(identities.length, 13);
  assert.deepEqual(
    [identities[4].kind_code, identities[4].identity_code],
    [0, 5], // Explicit Delta T identity equals its selector; DEFAULT is never a result.
  );
  happyExports.add("celestial_model_identities");
  const requiredText = M._celestial_capability_text(0, 9, 0, 0);
  assert(requiredText > 0);
  const textPtr = M._malloc(requiredText);
  try {
    assert.equal(M._celestial_capability_text(0, 9, textPtr, requiredText), requiredText);
    assert.equal(M.HEAPU8[textPtr + requiredText - 1], 0);
    const text = readCString(textPtr);
    assert(text.includes("Pluto") && text.includes("Meeus Chapter 37"));
    assert(text.includes("PLUTO95 supplies independent references, not the runtime evaluator"));
  } finally {
    M._free(textPtr);
  }
  happyExports.add("celestial_capability_text");

  for (const identity of identities) {
    const { kind_code, identity_code } = identity;
    assert(identity_code > 0, "identity 0 is input-only DEFAULT");
    const required = M._celestial_model_identity_text(kind_code, identity_code, 0, 0) >>> 0;
    assert.equal(M._celestial_last_error_kind(), 0);
    assert(required > 1);
    const ptr = M._malloc(required);
    assert.notEqual(ptr, 0);
    try {
      assert.equal(M._celestial_model_identity_text(kind_code, identity_code, ptr, required) >>> 0, required);
      assert.equal(M._celestial_last_error_kind(), 0);
      assert.equal(M.HEAPU8[ptr + required - 1], 0);
      const text = readCString(ptr);
      const expectedName = kind_code === 0 ? new RegExp(`Delta T algo${identity_code}`, "i")
        : kind_code === 1 ? [null, /Meeus/i, /IAU\s*1980/i][identity_code]
          : [null, /VSOP87D/i, /ELP2000/i, /VSOP87D/i, /PLUTO95|(?:Chapter|ch\.)\s*37/i, /mean.*node/i, /five.term/i][identity_code];
      assert(expectedName instanceof RegExp, "known model key");
      assert.match(text, expectedName, "identity text must name the model returned by that key");
    } finally { M._free(ptr); }
  }
  happyExports.add("celestial_model_identity_text");

  // #355: independent queries. Position count+fill follows the caller's target order with a
  // subset succeeding below the snapshot joint window; houses return by value with the
  // resolved model identities.
  const positionSelection = M._malloc(3 * 4);
  try {
    M.HEAPU32[positionSelection >> 2] = 9; // Pluto
    M.HEAPU32[(positionSelection >> 2) + 1] = 0; // Sun
    M.HEAPU32[(positionSelection >> 2) + 2] = 10; // mean ascending node
    const positionFill = eventFill("celestial_positions", "CelestialPositionRecord", [positionSelection, 3, 2451545.0]);
    assert.equal(positionFill.length, 3);
    assert.deepEqual(
      positionFill.map((position) => [position.target_code, position.present_fields & 2 ? 1 : 0]),
      [[9, 1], [0, 1], [10, 0]],
    );
    assert.equal(positionFill[2].source_code, 5); // CELESTIAL_POSITION_SOURCE_NODE_MEAN
    for (const position of positionFill) {
      assert(position.longitude_deg >= 0.0 && position.longitude_deg < 360.0);
      assert(Number.isFinite(position.latitude_deg));
    }
    happyExports.add("celestial_positions");
    const equatorial = eventFill("celestial_equatorial_positions", "CelestialEquatorialPositionRecord", [positionSelection, 3, 2451545.0]);
    assert.deepEqual(equatorial.map(({ target_code }) => target_code), [9, 0, 10]);
    assert.deepEqual(equatorial.map(({ source_code }) => source_code), positionFill.map(({ source_code }) => source_code));
    for (const point of equatorial) {
      assert(point.right_ascension_deg >= 0 && point.right_ascension_deg < 360);
      assert(point.declination_deg >= -90 && point.declination_deg <= 90);
      assert.equal(point.present_fields, point.target_code < 10 ? 2 : 0);
    }
    happyExports.add("celestial_equatorial_positions");
    for (const code of [14, 256, 257, 0x10000000, 0xFFFFFFFF]) {
      M.HEAPU32[positionSelection >> 2] = code;
      for (const exportName of ["celestial_positions", "celestial_equatorial_positions"]) {
        M.HEAPU32[countPtr >> 2] = 0xDEADBEEF;
        assert.equal(M[`_${exportName}`](positionSelection, 3, 2451545.0, countPtr, 0, 0), 0);
        assert.equal(M.HEAPU32[countPtr >> 2], 0);
        assert.equal(M._celestial_last_error_kind(), 1, "full-width target rejection");
      }
    }
  } finally {
    M._free(positionSelection);
  }

  const houseQuery = validSret(
    "celestial_house_query",
    [2024, 6, 1, 0.5, 1, 0, 0, 51.5, 0.0, 2],
  );
  assert.equal(houseQuery.house_system_code, 2);
  assert.equal(houseQuery.delta_t_identity_code, 5); // Algo5, never an unresolved DEFAULT
  assert.equal(houseQuery.nutation_identity_code, 2); // IAU 1980
  assert(houseQuery.jde_tt > houseQuery.jd_ut1);
  assert.equal(houseQuery.houses.cusps_deg.length, 12);
  for (const angle of [
    houseQuery.houses.ascendant_deg,
    houseQuery.houses.midheaven_deg,
    houseQuery.houses.descendant_deg,
    houseQuery.houses.imum_coeli_deg,
    ...houseQuery.houses.cusps_deg,
  ]) {
    assert(angle >= 0.0 && angle < 360.0);
  }
  // The snapshot joint window does not apply: 1885 resolves.
  const earlyHouse = rawSret("celestial_house_query", [1885, 1, 1, 0.0, 1, 5, 1, 51.5, 0.0, 0]);
  assert.equal(earlyHouse.valid, true);
  assert(earlyHouse.jde_tt < 2409543.875);
  assert.equal(earlyHouse.nutation_identity_code, 1); // MEEUS
  const houseCapability = validSret("celestial_house_capability", [4]);
  assert.equal(houseCapability.status_code, 1);
  assert.equal(houseCapability.delta_t_identity_code, 4);
  assert.equal(houseCapability.has_derived_tt_cutoff, true);
  assert.equal(houseCapability.derived_tt_end_jde_tt, 2464328.5);
  const defaultHouseCapability = validSret("celestial_house_capability", [0]);
  assert.equal(defaultHouseCapability.delta_t_identity_code, 5);
  assert.equal(defaultHouseCapability.has_derived_tt_cutoff, false);
} finally {
  M._free(oneSlot);
  M._free(countPtr);
}

const equation = validSret("celestial_equation_of_time", [2460463.0]);
assert(Math.abs(equation.value) < 5);
const apparent = validSret("celestial_apparent_solar_time", [2024, 6, 1, 0.5, 116.4]);
assert.deepEqual([apparent.year, apparent.month, apparent.day], [2024, 6, 1]);
assert(apparent.fraction > 0 && apparent.fraction < 1);
const sidereal = validSret("celestial_local_apparent_sidereal_time", [2460463.0, 120.0]);
assert(sidereal.value >= 0 && sidereal.value < 360);

const jieqi = validSret("celestial_query_jieqi_moment", [2024, 0]);
assert.deepEqual([jieqi.jq_idx, jieqi.y, jieqi.m], [0, 2024, 2]);
finite(jieqi.frac);
const JIEQI_NAMES = [
  "立春", "雨水", "惊蛰", "春分", "清明", "谷雨", "立夏", "小满", "芒种", "夏至", "小暑", "大暑",
  "立秋", "处暑", "白露", "秋分", "寒露", "霜降", "立冬", "小雪", "大雪", "冬至", "小寒", "大寒",
];
for (const [index, expected] of JIEQI_NAMES.entries()) {
  const required = M._celestial_get_jieqi_name(index, 0, 0) >>> 0;
  assert.equal(required, 7, "all current Jieqi names contain six UTF-8 bytes plus NUL");
  assert.equal(M._celestial_last_error_kind(), 0);
  const namePtr = M._malloc(required);
  assert.notEqual(namePtr, 0);
  try {
    assert.equal(M._celestial_get_jieqi_name(index, namePtr, required) >>> 0, required);
    assert.equal(readCString(namePtr), expected);
    assert.equal(M.HEAPU8[namePtr + required - 1], 0);
  } finally { M._free(namePtr); }
}
happyExports.add("celestial_get_jieqi_name");

const lunarRange = validSret("celestial_get_supported_lunar_year_range", [3]);
assert.deepEqual([lunarRange.start, lunarRange.end], [1600, 2199]);
const lunarInfo = validSret("celestial_get_lunar_year_info", [2, 2024]);
assert.deepEqual([lunarInfo.year, lunarInfo.month, lunarInfo.day], [2024, 2, 10]);
assert.equal(lunarInfo.leap_month, 0);
assert(lunarInfo.month_len > 0);
const lunarDate = validSret("celestial_gregorian_to_lunar", [1, 2023, 3, 22]);
assert.deepEqual(
  [lunarDate.year, lunarDate.month, lunarDate.is_leap, lunarDate.day],
  [2023, 2, true, 1],
);
const gregorianDate = validSret("celestial_lunar_to_gregorian", [1, 2023, 2, true, 1]);
assert.deepEqual([gregorianDate.year, gregorianDate.month, gregorianDate.day], [2023, 3, 22]);

for (const model of [0, 1, 2, 3, 4, 5]) {
  const result = validSret("celestial_delta_t", [2024.5, model]);
  finite(result.value);
  assert.equal(result.model_identity_code, model || 5);
}

const recordedNames = new Set();
const recordedFailure = (name, invoke) => {
  assert.equal(rawSret("celestial_ut1_to_jd", [2000, 1, 1, 0.5]).valid, true, "recording precondition");
  assert.equal(lastError(), "", `${name} starts with empty last_error`);
  assert.equal(invoke(), true, `${name} failure result`);
  const kind = M._celestial_last_error_kind();
  assert(kind > 0 && kind <= 4, `${name} records an error kind`);
  const message = lastError();
  assert.equal(M._celestial_last_error_kind(), kind, "message reader preserves kind");
  assert.equal(lastError(), message, "kind reader preserves message");
  recordedNames.add(name);
};
recordedFailure("celestial_ut1_to_jd", () => !rawSret("celestial_ut1_to_jd", [2024, 6, 1, 1.5]).valid);
recordedFailure("celestial_ut1_to_jde", () => !rawSret("celestial_ut1_to_jde", [2024, 13, 1, 0.5]).valid);
recordedFailure("celestial_jde_to_ut1", () => !rawSret("celestial_jde_to_ut1", [Number.NaN]).valid);
recordedFailure("celestial_moon_illumination", () => !rawSret("celestial_moon_illumination", [Number.NaN]).valid);
recordedFailure("celestial_moon_position_angle", () => !rawSret("celestial_moon_position_angle", [Number.NaN]).valid);
recordedFailure("celestial_moon_phase_moments", () => {
  const rootCount = M._malloc(4);
  try {
    M.HEAPU32[rootCount >> 2] = 0xDEADBEEF;
    const written = M._celestial_moon_phase_moments(2024, 4, rootCount, 0, 0);
    return written === 0 && M.HEAPU32[rootCount >> 2] === 0;
  } finally {
    M._free(rootCount);
  }
});
recordedFailure("celestial_local_apparent_sidereal_time", () => !rawSret("celestial_local_apparent_sidereal_time", [1000000.0, 0]).valid);

assert.equal(M._celestial_moon_phase_moments(2024, 0, 0, 0, 0), 0);
assert(lastError().length > 0, "moon_phase_moments rejects null root_count");
const nullProtocolCount = M._malloc(4);
try {
  M.HEAPU32[nullProtocolCount >> 2] = 0xDEADBEEF;
  assert.equal(M._celestial_moon_phase_moments(2024, 0, nullProtocolCount, 0, 1), 0);
  assert.equal(M.HEAPU32[nullProtocolCount >> 2], 0);
  assert(lastError().length > 0, "moon_phase_moments rejects null slots for a positive count");

  assert.equal(M._celestial_new_moons_in_year(2024, 0, 0, 0), 0);
  M.HEAPU32[nullProtocolCount >> 2] = 0xDEADBEEF;
  assert.equal(M._celestial_new_moons_in_year(2024, nullProtocolCount, 0, 1), 0);
  assert.equal(M.HEAPU32[nullProtocolCount >> 2], 0);
  assert.equal(M._celestial_solar_lon_roots(2024, 0.0, 0, 1), 0);
  assert.equal(M._celestial_new_moons_after_jde(2460463.0, 0, 1), 0);
} finally {
  M._free(nullProtocolCount);
}

const stale = lastError();
assert(stale.length > 0);
assert.equal(rawSret("celestial_sun_apparent_geocentric_coord", [Number.NaN]).valid, false);
assert(lastError().length > 0 && lastError() !== stale, "new recording failure replaces last_error");
assert.equal(rawSret("celestial_moon_illumination", [2448724.5]).valid, true);
assert.equal(lastError(), "", "successful recording call clears last_error");

recordedFailure("celestial_sun_apparent_geocentric_coord", () => !rawSret("celestial_sun_apparent_geocentric_coord", [Number.NaN]).valid);
recordedFailure("celestial_moon_apparent_geocentric_coord", () => !rawSret("celestial_moon_apparent_geocentric_coord", [Number.NaN]).valid);
recordedFailure("celestial_solar_lon_root_discriminant", () => !rawSret("celestial_solar_lon_root_discriminant", [2024, Number.NaN]).valid);
for (const longitude of [-1, 360, 720]) {
  recordedFailure("celestial_solar_lon_root_discriminant", () => !rawSret("celestial_solar_lon_root_discriminant", [2024, longitude]).valid);
  recordedFailure("celestial_solar_lon_roots", () => M._celestial_solar_lon_roots(2024, longitude, 0, 0) === 0);
  assert.equal(M._celestial_last_error_kind(), 1, "raw solar longitude domain");
}
assert.equal(rawSret("celestial_solar_lon_root_discriminant", [0, 0]).valid, false);
const edgeSlot = M._malloc(8);
const edgeCount = M._malloc(4);
try {
  recordedFailure("celestial_solar_lon_roots", () => M._celestial_solar_lon_roots(2024, Number.NaN, edgeSlot, 1) === 0);
  recordedFailure("celestial_new_moons_after_jde", () => M._celestial_new_moons_after_jde(Number.NaN, edgeSlot, 1) === 0);
  M.HEAPU32[edgeCount >> 2] = 0xDEADBEEF;
  recordedFailure("celestial_new_moons_in_year", () => M._celestial_new_moons_in_year(0, edgeCount, edgeSlot, 1) === 0);
  assert.equal(M.HEAPU32[edgeCount >> 2], 0);
} finally {
  M._free(edgeCount);
  M._free(edgeSlot);
}
recordedFailure("celestial_equation_of_time", () => !rawSret("celestial_equation_of_time", [Number.NaN]).valid);
recordedFailure("celestial_apparent_solar_time", () => !rawSret("celestial_apparent_solar_time", [2024, 6, 1, 0.5, 200]).valid);
recordedFailure("celestial_query_jieqi_moment", () => !rawSret("celestial_query_jieqi_moment", [2024, 24]).valid);
recordedFailure("celestial_get_supported_lunar_year_range", () => !rawSret("celestial_get_supported_lunar_year_range", [0]).valid);
recordedFailure("celestial_get_lunar_year_info", () => !rawSret("celestial_get_lunar_year_info", [9, 2024]).valid);
recordedFailure("celestial_gregorian_to_lunar", () => !rawSret("celestial_gregorian_to_lunar", [1, 2023, 13, 1]).valid);
recordedFailure("celestial_lunar_to_gregorian", () => !rawSret("celestial_lunar_to_gregorian", [1, 2024, 2, true, 1]).valid);
for (const model of [0, 1, 2, 3, 4, 5]) {
  recordedFailure("celestial_delta_t", () => !rawSret("celestial_delta_t", [Number.NaN, model]).valid);
}
for (const model of [6, 256, 257, 261, 0x10000005, 0xFFFFFFFF]) {
  recordedFailure("celestial_delta_t", () => !rawSret("celestial_delta_t", [2024.5, model]).valid);
  assert.equal(M._celestial_last_error_kind(), 1, "full-width invalid model is not aliased");
}
{
  const eventCount = M._malloc(4);
  try {
    M.HEAPU32[eventCount >> 2] = 0xDEADBEEF;
    recordedFailure("celestial_longitude_events", () => M._celestial_longitude_events(2, Number.NaN, 2460735.5, 2460785.5, eventCount, 0, 0) === 0);
    assert.equal(M.HEAPU32[eventCount >> 2], 0);
    M.HEAPU32[eventCount >> 2] = 0xDEADBEEF;
    recordedFailure("celestial_longitude_stations", () => M._celestial_longitude_stations(2, 2460785.5, 2460735.5, eventCount, 0, 0) === 0);
    assert.equal(M.HEAPU32[eventCount >> 2], 0);
    M.HEAPU32[eventCount >> 2] = 0xDEADBEEF;
    recordedFailure(
      "celestial_longitude_difference_events",
      () => M._celestial_longitude_difference_events(0, 0, 1.0, 2460676.5, 2460707.5, eventCount, 0, 0) === 0,
    );
    assert.equal(M.HEAPU32[eventCount >> 2], 0);
    M.HEAPU32[eventCount >> 2] = 0xDEADBEEF;
    recordedFailure("celestial_capabilities", () => M._celestial_capabilities(9, 0xFFFFFFFF, eventCount, 0, 0) === 0);
    assert.equal(M.HEAPU32[eventCount >> 2], 0);
    M.HEAPU32[eventCount >> 2] = 0xDEADBEEF;
    recordedFailure("celestial_model_identities", () => M._celestial_model_identities(9, eventCount, 0, 0) === 0);
    assert.equal(M.HEAPU32[eventCount >> 2], 0);
    recordedFailure("celestial_capability_text", () => M._celestial_capability_text(0, 256, 0, 0) === 0);
    recordedFailure("celestial_model_identity_text", () => M._celestial_model_identity_text(0, 0, 0, 0) === 0);
    recordedFailure("celestial_house_capability", () => !rawSret("celestial_house_capability", [256]).valid);
    M.HEAPU32[eventCount >> 2] = 0xDEADBEEF;
    recordedFailure("celestial_positions", () => M._celestial_positions(0, 1, 2451545.0, eventCount, 0, 0) === 0);
    assert.equal(M.HEAPU32[eventCount >> 2], 0);
    recordedFailure("celestial_equatorial_positions", () => M._celestial_equatorial_positions(0, 1, 2451545.0, eventCount, 0, 0) === 0);
    assert.equal(M.HEAPU32[eventCount >> 2], 0);
    recordedFailure("celestial_house_query", () => !rawSret("celestial_house_query", [2024, 6, 1, 0.5, 1, 0, 0, 89.9, 0.0, 2]).valid);
  } finally {
    M._free(eventCount);
  }
}

const tinyName = M._malloc(2);
try {
  M.HEAPU8[tinyName] = 0x41;
  M.HEAPU8[tinyName + 1] = 0x42;
  assert.equal(M._celestial_get_jieqi_name(0, tinyName, 2), 7);
  assert.equal(M._celestial_last_error_kind(), 0, "truncation is not failure");
  assert.equal(M.HEAPU8[tinyName + 1], 0, "truncated text is NUL-terminated");
  recordedFailure("celestial_get_jieqi_name", () => M._celestial_get_jieqi_name(0, 0, 16) === 0);
  recordedFailure("celestial_get_jieqi_name", () => M._celestial_get_jieqi_name(24, tinyName, 2) === 0);
} finally {
  M._free(tinyName);
}

assert.equal(rawSret("celestial_query_jieqi_moment", [40000, 13]).valid, false);
assert.equal(validSret("celestial_query_jieqi_moment", [2026, 13]).y, 2026, "module survives a translated C++ exception");

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
  const chart = validSret("celestial_chart_snapshot", chartArgs(changes));
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
assert.equal(chart.jd_ut1, validSret("celestial_ut1_to_jd", [2026, 1, 1, 0.5]).value);
assert.equal(chart.jde_tt, validSret("celestial_ut1_to_jde", [2026, 1, 1, 0.5]).value);
const explicitAlgo5 = validChart({ delta_t_model_code: 5 });
assert.deepEqual(usedChartPayload(chart), usedChartPayload(explicitAlgo5), "chart DEFAULT is exactly Algo5");
const explicitAlgo1 = validChart({ delta_t_model_code: 1 });
assert.notEqual(chart.jde_tt, explicitAlgo1.jde_tt, "DEFAULT must not enum-cast to Algo1");
assert.notEqual(chart.bodies[0].longitude_deg, explicitAlgo1.bodies[0].longitude_deg);

for (const model of [1, 2, 3, 4, 5]) {
  const selected = validChart({ delta_t_model_code: model });
  const delta = validSret("celestial_delta_t", [2026 + 0.5 / 365, model]).value;
  const expectedTt = validSret("celestial_ut1_to_jd", [2026, 1, 1, 0.5 + delta / 86400]).value;
  assert.equal(selected.jd_ut1, chart.jd_ut1, `UT1 ingress unchanged for model ${model}`);
  assert.equal(selected.jde_tt, expectedTt, `chart model ${model} uses the selected Delta T export`);
}

const utcChart = validChart({ civil_scale_code: 0 });
const utcAlgo1 = validChart({ civil_scale_code: 0, delta_t_model_code: 1 });
// #300's modern UTC replay: frozen Delta AT 37 s plus TT-TAI 32.184 s at this date.
assert.equal(utcChart.jde_tt, validSret("celestial_ut1_to_jd", [2026, 1, 1, 0.5 + 69.184 / 86400]).value);
assert.equal(utcChart.jde_tt, utcAlgo1.jde_tt, "UTC to TT is independent of Delta T model");
assert.notEqual(utcChart.jd_ut1, utcAlgo1.jd_ut1, "UTC to modelled UT1 uses the selected model");
assert.notEqual(utcChart.jd_ut1, chart.jd_ut1, "UTC is not silently interpreted as UT1");

const chartSun = validSret("celestial_sun_apparent_geocentric_coord", [chart.jde_tt]);
const chartMoon = validSret("celestial_moon_apparent_geocentric_coord", [chart.jde_tt]);
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
assert.equal(historical.jd_ut1, validSret("celestial_ut1_to_jd", [1900, 6, 1, 0.5]).value);

validChart({ year: 2000, month: 2, day: 29 });
validChart({ year: 1972, fraction: 0, civil_scale_code: 0 });
for (const longitude_deg of [-180, 180]) validChart({ latitude_deg: 0, longitude_deg });

let chartFailureCount = 0;
const invalidChart = (changes, expectedError) => {
  recordedFailure("celestial_chart_snapshot", () => !rawSret("celestial_chart_snapshot", chartArgs(changes)).valid);
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
  invalidChart({ delta_t_model_code }, `code ${delta_t_model_code}`);
  assert.equal(M._celestial_last_error_kind(), 1);
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
// The UTC witness depends on Algo 5's UT1; rederive the location after a refit.
const unresolvedAscendant = {
  year: 2026, month: 1, day: 1, fraction: 43205 / 86400, civil_scale_code: 0,
  latitude_deg: 66.561856230963841, longitude_deg: -11.176256364511062,
  house_system_code: 0, delta_t_model_code: 5,
};
invalidChart(unresolvedAscendant, "Ascendant");
validChart({ ...unresolvedAscendant, latitude_deg: unresolvedAscendant.latitude_deg + 0.000001 });
assert.equal(rawSret("celestial_chart_snapshot", chartArgs({ fraction: NaN })).valid, false);
const chartError = lastError();
assert(chartError.includes("fraction"));
assert.equal(rawSret("celestial_chart_snapshot", chartArgs({ civil_scale_code: 257 })).valid, false);
assert(lastError().includes("civil_scale_code") && lastError() !== chartError, "chart failure replaces last_error");
assert.equal(lastError(), lastError(), "last_error itself preserves the recorded chart failure");
validChart();

let goldenCount = 0;
for (const year of [-2147483648, -63510, -32768, 32768, 65537, 67421, 67562, 2147483647]) {
  for (const [name, args] of [
    ["celestial_ut1_to_jd", [year, 1, 1, 0.5]],
    ["celestial_ut1_to_jde", [year, 1, 1, 0.5]],
    ["celestial_apparent_solar_time", [year, 1, 1, 0.5, 0]],
    ["celestial_gregorian_to_lunar", [3, year, 6, 1]],
  ]) {
    recordedFailure(name, () => !rawSret(name, args).valid);
    assert(lastError().includes(String(year)), `${name} lost original year ${year}`);
  }
}
for (const year of [-2147483648, -63510, 0, 32767, 32768, 65537, 67562, 2147483647]) {
  recordedFailure("celestial_solar_lon_root_discriminant", () => !rawSret("celestial_solar_lon_root_discriminant", [year, 0]).valid);
  assert(lastError().includes(String(year)) && lastError().includes("[1, 32766]"), "solar annual-year context");
  const slots = M._malloc(16);
  try {
    M.HEAPF64[slots >> 3] = 77;
    M.HEAPF64[(slots >> 3) + 1] = 88;
    recordedFailure("celestial_solar_lon_roots", () => M._celestial_solar_lon_roots(year, 0, slots, 2) === 0);
    assert.deepEqual(readDoubles(slots, 2), [77, 88], "failed solar query changed slots");
    assert(lastError().includes(String(year)) && lastError().includes("[1, 32766]"), "solar fill annual-year context");
  } finally {
    M._free(slots);
  }
}
const diagnosticCount = M._malloc(4);
try {
  M.HEAPU32[diagnosticCount >> 2] = 17;
  recordedFailure("celestial_moon_phase_moments", () => M._celestial_moon_phase_moments(32767, 0, diagnosticCount, 0, 0) === 0);
  assert.equal(M.HEAPU32[diagnosticCount >> 2], 0);
  assert(lastError().includes("32767") && lastError().includes("[1, 32766]"), "window diagnostic regression");
} finally {
  M._free(diagnosticCount);
}
finite(validSret("celestial_ut1_to_jd", [32767, 1, 1, 0.5]).value);
assert.equal(lastError(), "", "valid scalar 32767 recovers and clears errors");
validSret("celestial_ut1_to_jde", [32767, 1, 1, 0.5]);
assert.equal(validSret("celestial_apparent_solar_time", [32767, 1, 1, 0.5, 0]).year, 32767);
validSret("celestial_gregorian_to_lunar", [3, 2026, 6, 1]);
assert.equal(lastError(), "");
console.log("PASS raw original-year rejection/context; annual windows; untouched slots; scalar 32767/recovery");

for (const point of golden.sections.jieqi.entries) {
  const value = validSret("celestial_query_jieqi_moment", [point.year, point.idx]);
  assert.deepEqual([value.jq_idx, value.y, value.m, value.d], [point.idx, point.y, point.m, point.d]);
  close(value.frac, bitsOf(point.frac_bits), MAX_MOMENT_DIFF_DAYS, "jieqi frac");
  ++goldenCount;
}
for (const point of golden.sections.moon.entries) {
  const value = validSret("celestial_moon_illumination", [bitsOf(point.jde_bits)]);
  close(value.illumination, bitsOf(point.illumination_bits), MAX_LUNAR_VALUE_DIFF, "moon illumination");
  close(value.elongation_deg, bitsOf(point.elongation_deg_bits), MAX_LUNAR_VALUE_DIFF, "moon elongation");
  ++goldenCount;
}
for (const point of golden.sections.sidereal.entries) {
  const value = validSret("celestial_local_apparent_sidereal_time", [bitsOf(point.jd_ut1_bits), point.longitude]);
  close(value.value, bitsOf(point.value_bits), MAX_SIDEREAL_DIFF_DEG, "sidereal");
  ++goldenCount;
}
for (const point of golden.sections.moon_position_angle.entries) {
  const value = validSret("celestial_moon_position_angle", [bitsOf(point.jde_bits)]);
  close(value.angle_deg, bitsOf(point.angle_deg_bits), MAX_LUNAR_VALUE_DIFF, "moon position angle");
  ++goldenCount;
}
const phaseCache = new Map();
const phaseMoments = (year, phaseKind) => {
  const rootCount = M._malloc(4);
  try {
    assert.equal(M._celestial_moon_phase_moments(year, phaseKind, rootCount, 0, 0), 0);
    const count = M.HEAPU32[rootCount >> 2];
    assert(count > 0);
    const slots = M._malloc(count * 8);
    try {
      assert.equal(M._celestial_moon_phase_moments(year, phaseKind, rootCount, slots, count), count);
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

// Event fixtures (#303): the same six queries the C ABI and Python consumers replay.
// WASM runs musl's libm against the native goldens: times take the existing moment
// margin; codes stay exact; angles scale by the Moon's ~15.5 deg/day through 1e-8 days
// (<= 1.6e-7 deg, so 2e-7), and the seven-point rate divides that by its 0.75-day stencil
// (<= 2.7e-7 deg/day, so 1e-5 leaves ~40x headroom).
const MAX_EVENT_ANGLE_DIFF_DEG = 2e-7;
const MAX_EVENT_RATE_DIFF_DEG_PER_TT_DAY = 1e-5;
const eventQuery = (exportName, recordLayout, entry, args) => {
  const recordSize = manifest.layouts[recordLayout].size;
  const countPtr = M._malloc(4);
  try {
    assert.equal(M[`_${exportName}`](...args, countPtr, 0, 0), 0);
    const count = M.HEAPU32[countPtr >> 2];
    assert.equal(count, entry.events.length, `${entry.name} count`);
    const slots = M._malloc(Math.max(count * recordSize, 1));
    try {
      const fillCount = M._malloc(4);
      try {
        assert.equal(M[`_${exportName}`](...args, fillCount, slots, count), count);
        assert.equal(M.HEAPU32[fillCount >> 2], count);
        return Array.from({ length: count }, (_, index) => readLayout(slots + index * recordSize, recordLayout));
      } finally {
        M._free(fillCount);
      }
    } finally {
      M._free(slots);
    }
  } finally {
    M._free(countPtr);
  }
};
for (const entry of golden.sections.longitude_events.entries) {
  const events = eventQuery(
    "celestial_longitude_events", "CelestialLongitudeEventRecord", entry,
    [entry.target_code, entry.target_longitude_deg, entry.start_jde_tt, entry.end_jde_tt],
  );
  events.forEach((event, index) => {
    const recorded = entry.events[index];
    assert.equal(event.direction_code, recorded.direction_code, `${entry.name}[${index}] direction`);
    close(event.jde_tt, bitsOf(recorded.jde_tt_bits), MAX_MOMENT_DIFF_DAYS, `${entry.name}[${index}] jde_tt`);
    close(event.longitude_deg, bitsOf(recorded.longitude_deg_bits), MAX_EVENT_ANGLE_DIFF_DEG, `${entry.name}[${index}] longitude`);
    close(
      event.longitude_rate_deg_per_tt_day,
      bitsOf(recorded.longitude_rate_deg_per_tt_day_bits),
      MAX_EVENT_RATE_DIFF_DEG_PER_TT_DAY,
      `${entry.name}[${index}] rate`,
    );
  });
  ++goldenCount;
}
for (const entry of golden.sections.longitude_stations.entries) {
  const stations = eventQuery(
    "celestial_longitude_stations", "CelestialStationEventRecord", entry, [entry.target_code, entry.start_jde_tt, entry.end_jde_tt],
  );
  stations.forEach((station, index) => {
    const recorded = entry.events[index];
    assert.equal(station.motion_before_code, recorded.motion_before_code, `${entry.name}[${index}] motion_before`);
    assert.equal(station.motion_after_code, recorded.motion_after_code, `${entry.name}[${index}] motion_after`);
    close(station.jde_tt, bitsOf(recorded.jde_tt_bits), MAX_MOMENT_DIFF_DAYS, `${entry.name}[${index}] jde_tt`);
    close(station.longitude_deg, bitsOf(recorded.longitude_deg_bits), MAX_EVENT_ANGLE_DIFF_DEG, `${entry.name}[${index}] longitude`);
    close(
      station.longitude_rate_deg_per_tt_day,
      bitsOf(recorded.longitude_rate_deg_per_tt_day_bits),
      MAX_EVENT_RATE_DIFF_DEG_PER_TT_DAY,
      `${entry.name}[${index}] rate`,
    );
  });
  ++goldenCount;
}
for (const entry of golden.sections.longitude_difference_events.entries) {
  const differences = eventQuery(
    "celestial_longitude_difference_events", "CelestialLongitudeDifferenceEventRecord", entry,
    [entry.first_target_code, entry.second_target_code, entry.target_difference_deg, entry.start_jde_tt, entry.end_jde_tt],
  );
  differences.forEach((difference, index) => {
    const recorded = entry.events[index];
    assert.equal(difference.direction_code, recorded.direction_code, `${entry.name}[${index}] direction`);
    close(difference.jde_tt, bitsOf(recorded.jde_tt_bits), MAX_MOMENT_DIFF_DAYS, `${entry.name}[${index}] jde_tt`);
    close(difference.longitude_a_deg, bitsOf(recorded.longitude_a_deg_bits), MAX_EVENT_ANGLE_DIFF_DEG, `${entry.name}[${index}] longitude_a`);
    close(difference.longitude_b_deg, bitsOf(recorded.longitude_b_deg_bits), MAX_EVENT_ANGLE_DIFF_DEG, `${entry.name}[${index}] longitude_b`);
    close(
      difference.signed_difference_deg, bitsOf(recorded.signed_difference_deg_bits), MAX_EVENT_ANGLE_DIFF_DEG,
      `${entry.name}[${index}] signed_difference`,
    );
  });
  ++goldenCount;
}

const expectedExports = BINDINGS.map(({ cName }) => cName);
assert.deepEqual(
  [...recordedNames].sort(),
  BINDINGS.filter(({ readsLastError }) => readsLastError).map(({ cName }) => cName).sort(),
  "all recording exports exercised a failure",
);
assert.equal(goldenCount, 395, "golden replay count");
assert.equal(BINDINGS.length, 36, "38 legacy exports minus logging and five Delta T siblings, plus four new exports");
assert.equal(Object.keys(manifest.layouts).length, 28, "26 legacy records plus equatorial position and civil-house capability");
assert.deepEqual([...happyExports].sort(), expectedExports.sort(), "all 36 exports executed successfully");
assert.deepEqual([...seenLayouts].sort(), Object.keys(manifest.layouts).sort(), "all 28 layouts decoded");
assert.deepEqual(
  [...new Set(manifest.exports.map(({ protocol }) => protocol.kind).filter((kind) => kind.endsWith("fill")))].sort(),
  ["companion-fill", "count-fill", "requested-fill"],
  "three count/fill protocol classes",
);

console.log("PASS raw exports 36/36; layouts 28/28; recording seams 34/34");
console.log(`PASS private chart sret: order, absence, units, systems, scales, DEFAULT/models; rejected inputs ${chartFailureCount}`);
console.log("PASS caller string + borrowed string + three count/fill classes + legal zero");
console.log("PASS memory growth refreshed HEAP views; translated exception survived");
console.log(`PASS shared binding golden replay ${goldenCount}/395`);
