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

import createWasmModule from "./celestial-jieqi.mjs";

import { BINDINGS, CHART_CODES, LAYOUTS } from "./bindings.mjs";
import {
  booleanValue,
  civilDateTime,
  civilDateTimeResult,
  enumValue,
  finiteNumber,
  gregorianDate,
  rangedInteger,
  rangedNumber,
  requiredRecord,
} from "./validation.mjs";

const WASM_URL = new URL("./celestial-jieqi.wasm", import.meta.url);
const UTF8 = new TextDecoder();
const MAX_CALENDAR_YEAR = 32_766;
const MAX_NEW_MOON_COUNT = 4_096;
const JIEQI_NAME_BYTES = 16;

const LOG_VERBOSITY = Object.freeze({ none: 0, info: 1, debug: 2 });
const MOON_PHASE = Object.freeze({ new: 0, firstQuarter: 1, full: 2, lastQuarter: 3 });
const DELTA_T_MODEL = Object.freeze({
  default: "delta_t",
  algo1: "delta_t_algo1",
  algo2: "delta_t_algo2",
  algo3: "delta_t_algo3",
  algo4: "delta_t_algo4",
  algo5: "delta_t_algo5",
});
const LUNAR_ALGORITHM = Object.freeze({ algo1: 1, algo2: 2, algo3: 3 });
const BINDING_BY_NAME = Object.freeze(Object.fromEntries(BINDINGS.map((entry) => [entry.cName, entry])));

export const Jieqi = Object.freeze({
  LICHUN: 0,
  YUSHUI: 1,
  JINGZHE: 2,
  CHUNFEN: 3,
  QINGMING: 4,
  GUYU: 5,
  LIXIA: 6,
  XIAOMAN: 7,
  MANGZHONG: 8,
  XIAZHI: 9,
  XIAOSHU: 10,
  DASHU: 11,
  LIQIU: 12,
  CHUSHU: 13,
  BAILU: 14,
  QIUFEN: 15,
  HANLU: 16,
  SHUANGJIANG: 17,
  LIDONG: 18,
  XIAOXUE: 19,
  DAXUE: 20,
  DONGZHI: 21,
  XIAOHAN: 22,
  DAHAN: 23,
});

let moduleInstance;
let initialization;

export class CelestialError extends Error {
  constructor(operation, message, recorded) {
    super(message);
    this.name = "CelestialError";
    this.operation = operation;
    this.recorded = recorded;
  }
}

export function init() {
  if (initialization === undefined) {
    const locateFile = (path, prefix) => path.endsWith(".wasm") ? WASM_URL.href : `${prefix}${path}`;
    initialization = createWasmModule({ locateFile }).then(
      (value) => {
        moduleInstance = value;
      },
      (error) => {
        initialization = undefined;
        throw error;
      },
    );
  }
  return initialization;
}

const requireModule = (operation) => {
  if (moduleInstance === undefined) {
    throw new CelestialError(operation, `Call and await init() before ${operation}().`, false);
  }
  return moduleInstance;
};

const bindingOf = (cName) => {
  const entry = BINDING_BY_NAME[cName];
  if (entry === undefined) throw new Error(`Unknown internal binding: ${cName}`);
  return entry;
};

const lunarYear = (nativeAlgorithm, year, name, operation) => {
  const range = callSret("get_supported_lunar_year_range", operation, [nativeAlgorithm]);
  return rangedInteger(year, name, range.start, range.end);
};

const readField = (M, ptr, field) => {
  const address = ptr + field.offset;
  switch (field.type) {
    case "bool": return M.HEAPU8[address] !== 0;
    case "uint8_t": return M.HEAPU8[address];
    case "uint16_t": return M.HEAPU16[address >> 1];
    case "int32_t": return M.HEAP32[address >> 2];
    case "uint32_t": return M.HEAPU32[address >> 2];
    case "double": return M.HEAPF64[address >> 3];
    default: throw new Error(`Unknown internal field type: ${field.type}`);
  }
};

const readLayout = (M, ptr, layoutName) => Object.fromEntries(
  LAYOUTS[layoutName].fields.map((field) => [field.name, readField(M, ptr, field)]),
);

const readCString = (M, ptr) => {
  let end = ptr;
  while (M.HEAPU8[end] !== 0) ++end;
  return UTF8.decode(M.HEAPU8.slice(ptr, end));
};

const fail = (M, binding, operation, fallback = `${operation} failed.`) => {
  const message = binding.readsLastError ? M.ccall("last_error", "string", [], []) : "";
  throw new CelestialError(operation, message || fallback, message.length > 0);
};

const callSret = (cName, operation, args) => {
  const M = requireModule(operation);
  const binding = bindingOf(cName);
  const layoutName = binding.result.slice("sret:".length);
  const layout = LAYOUTS[layoutName];
  const ptr = M._malloc(layout.size);

  try {
    M[binding.wasmName](ptr, ...args);
    const value = readLayout(M, ptr, layoutName);
    if (!value.valid) fail(M, binding, operation);
    return value;
  } finally {
    M._free(ptr);
  }
};

const readDoubles = (M, ptr, count) => Array.from(
  { length: count },
  (_, index) => M.HEAPF64[(ptr >> 3) + index],
);

const countThenFill = (cName, operation, args) => {
  const M = requireModule(operation);
  const binding = bindingOf(cName);
  const countPtr = M._malloc(4);

  try {
    M.HEAPU32[countPtr >> 2] = 0;
    const queried = M[binding.wasmName](...args, countPtr, 0, 0);
    const count = M.HEAPU32[countPtr >> 2];
    if (queried !== 0) {
      fail(M, binding, operation, `${operation} returned an inconsistent native result.`);
    }
    if (count === 0) fail(M, binding, operation);

    const slots = M._malloc(count * 8);
    try {
      const written = M[binding.wasmName](...args, countPtr, slots, count);
      if (written !== count || M.HEAPU32[countPtr >> 2] !== count) {
        fail(M, binding, operation, `${operation} returned an inconsistent native result.`);
      }
      return readDoubles(M, slots, count);
    } finally {
      M._free(slots);
    }
  } finally {
    M._free(countPtr);
  }
};

const setLogVerbosity = (level) => {
  const operation = "config.setLogVerbosity";
  const M = requireModule(operation);
  const nativeLevel = enumValue(level, "level", LOG_VERBOSITY);
  if (!M._set_log_verbosity(nativeLevel)) fail(M, bindingOf("set_log_verbosity"), operation);
};

const ut1ToJd = (ut1) => {
  const operation = "time.ut1ToJd";
  requireModule(operation);
  const value = civilDateTime(ut1, "ut1");
  return callSret("ut1_to_jd", operation, [value.year, value.month, value.day, value.fraction]).value;
};

const ut1ToJde = (ut1) => {
  const operation = "time.ut1ToJde";
  requireModule(operation);
  const value = civilDateTime(ut1, "ut1");
  return callSret("ut1_to_jde", operation, [value.year, value.month, value.day, value.fraction]).value;
};

const jdeToUt1 = (jde) => {
  const operation = "time.jdeToUt1";
  requireModule(operation);
  const value = callSret("jde_to_ut1", operation, [finiteNumber(jde, "jde")]);
  return civilDateTimeResult(value);
};

const localApparentSiderealTime = (jdUt1, longitudeDeg) => {
  const operation = "time.localApparentSiderealTime";
  requireModule(operation);
  const jd = finiteNumber(jdUt1, "jdUt1");
  const longitude = rangedNumber(longitudeDeg, "longitudeDeg", -180, 180);
  return callSret("local_apparent_sidereal_time", operation, [jd, longitude]).value;
};

const deltaT = (year, model = "default") => {
  const operation = "time.deltaT";
  requireModule(operation);
  const decimalYear = finiteNumber(year, "year");
  const cName = enumValue(model, "model", DELTA_T_MODEL);
  if (model === "algo1" && decimalYear < -4000) throw new RangeError("year must be at least -4000 for algo1.");
  if (model === "algo3" && decimalYear >= 3000) throw new RangeError("year must be less than 3000 for algo3.");
  if (model === "algo4" && decimalYear >= 2035) throw new RangeError("year must be less than 2035 for algo4.");
  return callSret(cName, operation, [decimalYear]).value;
};

const sunApparentGeocentricCoordinate = (jde) => {
  const operation = "sun.apparentGeocentricCoordinate";
  requireModule(operation);
  const value = callSret("sun_apparent_geocentric_coord", operation, [finiteNumber(jde, "jde")]);
  return { longitudeDeg: value.lon, latitudeDeg: value.lat, radiusAu: value.r };
};

const longitudeCrossings = (year, longitudeDeg) => {
  const operation = "sun.longitudeCrossings";
  const M = requireModule(operation);
  const checkedYear = rangedInteger(year, "year", 1, MAX_CALENDAR_YEAR);
  const longitude = rangedNumber(longitudeDeg, "longitudeDeg", 0, 360, false);
  const discriminant = callSret("solar_lon_root_discriminant", operation, [checkedYear, longitude]);
  if (discriminant.count === 0) return [];

  const slots = M._malloc(discriminant.count * 8);
  try {
    const written = M._solar_lon_roots(checkedYear, longitude, slots, discriminant.count);
    if (written !== discriminant.count) {
      fail(M, bindingOf("solar_lon_roots"), operation, `${operation} returned an inconsistent native result.`);
    }
    return readDoubles(M, slots, written);
  } finally {
    M._free(slots);
  }
};

const equationOfTime = (jde) => {
  const operation = "sun.equationOfTime";
  requireModule(operation);
  return callSret("equation_of_time", operation, [finiteNumber(jde, "jde")]).value;
};

const apparentSolarTime = (utc, longitudeDeg) => {
  const operation = "sun.apparentSolarTime";
  requireModule(operation);
  const value = civilDateTime(utc, "utc");
  const longitude = rangedNumber(longitudeDeg, "longitudeDeg", -180, 180);
  const result = callSret(
    "apparent_solar_time",
    operation,
    [value.year, value.month, value.day, value.fraction, longitude],
  );
  return civilDateTimeResult(result);
};

const moonApparentGeocentricCoordinate = (jde) => {
  const operation = "moon.apparentGeocentricCoordinate";
  requireModule(operation);
  const value = callSret("moon_apparent_geocentric_coord", operation, [finiteNumber(jde, "jde")]);
  return { longitudeDeg: value.lon, latitudeDeg: value.lat, distanceKm: value.r };
};

const illumination = (jde) => {
  const operation = "moon.illumination";
  requireModule(operation);
  const value = callSret("moon_illumination", operation, [finiteNumber(jde, "jde")]);
  return { fraction: value.illumination, elongationDeg: value.elongation_deg };
};

const brightLimbPositionAngle = (jde) => {
  const operation = "moon.brightLimbPositionAngle";
  requireModule(operation);
  return callSret("moon_position_angle", operation, [finiteNumber(jde, "jde")]).angle_deg;
};

const phaseMoments = (year, phase) => {
  const operation = "moon.phaseMoments";
  requireModule(operation);
  const checkedYear = rangedInteger(year, "year", 1, MAX_CALENDAR_YEAR);
  const phaseKind = enumValue(phase, "phase", MOON_PHASE);
  return countThenFill("moon_phase_moments", operation, [checkedYear, phaseKind]);
};

const newMoonsAfter = (jde, count) => {
  const operation = "moon.newMoonsAfter";
  const M = requireModule(operation);
  const start = finiteNumber(jde, "jde");
  const checkedCount = rangedInteger(count, "count", 0, MAX_NEW_MOON_COUNT);
  if (checkedCount === 0) return [];

  const slots = M._malloc(checkedCount * 8);
  if (slots === 0) {
    throw new CelestialError(operation, `${operation} failed to allocate the WASM output buffer.`, false);
  }
  try {
    const written = M._new_moons_after_jde(start, slots, checkedCount);
    if (written !== checkedCount) fail(M, bindingOf("new_moons_after_jde"), operation);
    return readDoubles(M, slots, written);
  } finally {
    M._free(slots);
  }
};

const newMoonsInYear = (year) => {
  const operation = "moon.newMoonsInYear";
  requireModule(operation);
  const checkedYear = rangedInteger(year, "year", 1, MAX_CALENDAR_YEAR);
  return countThenFill("new_moons_in_year", operation, [checkedYear]);
};

const jieqiMoment = (year, index) => {
  const operation = "jieqi.moment";
  requireModule(operation);
  const checkedYear = rangedInteger(year, "year", 401, MAX_CALENDAR_YEAR);
  const checkedIndex = rangedInteger(index, "index", 0, 23);
  const value = callSret("query_jieqi_moment", operation, [checkedYear, checkedIndex]);
  return {
    jieqi: value.jq_idx,
    momentUt1: civilDateTimeResult({ year: value.y, month: value.m, day: value.d, fraction: value.frac }),
  };
};

const jieqiName = (index) => {
  const operation = "jieqi.name";
  const M = requireModule(operation);
  const checkedIndex = rangedInteger(index, "index", 0, 23);
  const ptr = M._malloc(JIEQI_NAME_BYTES);

  try {
    if (!M._get_jieqi_name(checkedIndex, ptr, JIEQI_NAME_BYTES)) {
      fail(M, bindingOf("get_jieqi_name"), operation);
    }
    return readCString(M, ptr);
  } finally {
    M._free(ptr);
  }
};

const supportedYearRange = (algorithm) => {
  const operation = "lunar.supportedYearRange";
  requireModule(operation);
  const nativeAlgorithm = enumValue(algorithm, "algorithm", LUNAR_ALGORITHM);
  const value = callSret("get_supported_lunar_year_range", operation, [nativeAlgorithm]);
  return { start: value.start, end: value.end };
};

const yearInfo = (algorithm, year) => {
  const operation = "lunar.yearInfo";
  requireModule(operation);
  const nativeAlgorithm = enumValue(algorithm, "algorithm", LUNAR_ALGORITHM);
  const checkedYear = lunarYear(nativeAlgorithm, year, "year", operation);
  const value = callSret("get_lunar_year_info", operation, [nativeAlgorithm, checkedYear]);
  const monthCount = value.leap_month === 0 ? 12 : 13;
  const monthLengths = Array.from(
    { length: monthCount },
    (_, index) => ((value.month_len >> index) & 1) === 1 ? 30 : 29,
  );
  return {
    firstDay: { year: value.year, month: value.month, day: value.day },
    leapMonth: value.leap_month === 0 ? null : value.leap_month,
    monthLengths,
  };
};

const fromGregorian = (algorithm, date) => {
  const operation = "lunar.fromGregorian";
  requireModule(operation);
  const nativeAlgorithm = enumValue(algorithm, "algorithm", LUNAR_ALGORITHM);
  const value = gregorianDate(date, "date");
  const result = callSret(
    "gregorian_to_lunar",
    operation,
    [nativeAlgorithm, value.year, value.month, value.day],
  );
  return { year: result.year, month: result.month, day: result.day, isLeap: result.is_leap };
};

const toGregorian = (algorithm, date) => {
  const operation = "lunar.toGregorian";
  requireModule(operation);
  const nativeAlgorithm = enumValue(algorithm, "algorithm", LUNAR_ALGORITHM);
  requiredRecord(date, "date", ["year", "month", "day", "isLeap"], ["fraction"]);
  const year = lunarYear(nativeAlgorithm, date.year, "date.year", operation);
  const month = rangedInteger(date.month, "date.month", 1, 12);
  const day = rangedInteger(date.day, "date.day", 1, 30);
  const isLeap = booleanValue(date.isLeap, "date.isLeap");
  const value = callSret("lunar_to_gregorian", operation, [nativeAlgorithm, year, month, isLeap, day]);
  return { year: value.year, month: value.month, day: value.day };
};

const malformedChart = (field) => {
  throw new CelestialError("chart.snapshot", `Invalid native chart field: ${field}.`, false);
};

const chartFinite = (value, field) => {
  if (!Number.isFinite(value)) malformedChart(field);
  return value;
};

const chartLongitude = (value, field) => {
  chartFinite(value, field);
  if (value < 0 || value >= 360) malformedChart(field);
  return value;
};

const chartField = (layout, name) => LAYOUTS[layout].fields.find((field) => field.name === name);
const readChartField = (M, ptr, layout, name) => readField(M, ptr, chartField(layout, name));

const readChartSnapshot = (M, ptr) => {
  const readSnapshot = (name) => readChartField(M, ptr, "ChartSnapshotV1", name);
  const bodiesPtr = ptr + chartField("ChartSnapshotV1", "bodies").offset;
  const bodies = Object.entries(CHART_CODES.targets).map(([target, code], index) => {
    const bodyPtr = bodiesPtr + index * LAYOUTS.ChartBodyV1.size;
    const readBody = (name) => readChartField(M, bodyPtr, "ChartBodyV1", name);
    const isNode = code >= CHART_CODES.targets.meanAscending;
    const present = CHART_CODES.present.latitude | (isNode ? 0 : CHART_CODES.present.distance);
    if (readBody("target_code") !== code) malformedChart(`bodies[${index}].target_code`);
    if (readBody("present_fields") !== present) malformedChart(`${target}.present_fields`);
    const latitudeDeg = chartFinite(readBody("latitude_deg"), `${target}.latitude_deg`);
    if (latitudeDeg < -90 || latitudeDeg > 90 || (isNode && latitudeDeg !== 0)) {
      malformedChart(`${target}.latitude_deg`);
    }
    // V1 node distance storage is absent, not a numeric sentinel.
    const distanceAu = isNode ? null : chartFinite(readBody("distance_au"), `${target}.distance_au`);
    if (!isNode && distanceAu <= 0) malformedChart(`${target}.distance_au`);
    return {
      target,
      longitudeDeg: chartLongitude(readBody("longitude_deg"), `${target}.longitude_deg`),
      latitudeDeg,
      distanceAu,
      longitudeRateDegPerTtDay: chartFinite(
        readBody("longitude_rate_deg_per_tt_day"),
        `${target}.longitude_rate_deg_per_tt_day`,
      ),
    };
  });

  const housesPtr = ptr + chartField("ChartSnapshotV1", "houses").offset;
  const houseAngle = (name) => chartLongitude(
    readChartField(M, housesPtr, "ChartHousesV1", name),
    `houses.${name}`,
  );
  const cuspField = chartField("ChartHousesV1", "cusps_deg");
  const cuspCount = Number(cuspField.type.match(/\[(\d+)\]$/)[1]);
  return {
    times: {
      jdUt1: chartFinite(readSnapshot("jd_ut1"), "jd_ut1"),
      jdeTt: chartFinite(readSnapshot("jde_tt"), "jde_tt"),
    },
    bodies,
    houses: {
      ascendantDeg: houseAngle("ascendant_deg"),
      midheavenDeg: houseAngle("midheaven_deg"),
      descendantDeg: houseAngle("descendant_deg"),
      imumCoeliDeg: houseAngle("imum_coeli_deg"),
      cuspsDeg: readDoubles(M, housesPtr + cuspField.offset, cuspCount)
        .map((value, index) => chartLongitude(value, `houses.cusps_deg[${index}]`)),
    },
  };
};

const snapshot = (civil, options) => {
  const operation = "chart.snapshot";
  const M = requireModule(operation);
  const value = civilDateTime(civil, "civil");
  rangedInteger(value.year, "civil.year", 1885, 2099);
  requiredRecord(options, "options", ["scale", "location", "system"], []);
  if (Reflect.ownKeys(options).some((key) => !["scale", "location", "system", "model"].includes(key))) {
    throw new TypeError("options accepts only scale, location, system and model.");
  }
  const hasModel = Object.hasOwn(options, "model");
  if (!hasModel && "model" in options) {
    throw new TypeError("options.model must be an own property.");
  }
  const scale = enumValue(options.scale, "options.scale", CHART_CODES.scales);
  if (scale === CHART_CODES.scales.utc && value.year < 1972) {
    throw new RangeError("UTC chart input must be on or after 1972-01-01.");
  }
  const location = requiredRecord(
    options.location,
    "options.location",
    ["latitudeDeg", "longitudeDeg"],
    [],
  );
  const latitude = finiteNumber(location.latitudeDeg, "options.location.latitudeDeg");
  if (latitude <= -90 || latitude >= 90) {
    throw new RangeError("options.location.latitudeDeg must be in (-90, 90).");
  }
  const longitude = rangedNumber(location.longitudeDeg, "options.location.longitudeDeg", -180, 180);
  const system = enumValue(options.system, "options.system", CHART_CODES.systems);
  const model = hasModel ? options.model : undefined;
  const modelCode = enumValue(model === undefined ? "default" : model, "options.model", CHART_CODES.models);

  const binding = bindingOf("chart_snapshot_v1");
  const ptr = M._malloc(LAYOUTS.ChartSnapshotV1.size);
  if (ptr === 0) {
    throw new CelestialError(operation, `${operation} failed to allocate the WASM output buffer.`, false);
  }
  try {
    M[binding.wasmName](
      ptr,
      value.year,
      value.month,
      value.day,
      value.fraction,
      scale,
      latitude,
      longitude,
      system,
      modelCode,
    );
    const valid = M.HEAPU8[ptr + chartField("ChartSnapshotV1", "valid").offset];
    if (valid === 0) fail(M, binding, operation);
    if (valid !== 1) malformedChart("valid");
    return readChartSnapshot(M, ptr);
  } finally {
    M._free(ptr);
  }
};

export const config = Object.freeze({ setLogVerbosity });
export const time = Object.freeze({ ut1ToJd, ut1ToJde, jdeToUt1, localApparentSiderealTime, deltaT });
export const sun = Object.freeze({
  apparentGeocentricCoordinate: sunApparentGeocentricCoordinate,
  longitudeCrossings,
  equationOfTime,
  apparentSolarTime,
});
export const moon = Object.freeze({
  apparentGeocentricCoordinate: moonApparentGeocentricCoordinate,
  illumination,
  brightLimbPositionAngle,
  phaseMoments,
  newMoonsAfter,
  newMoonsInYear,
});
export const jieqi = Object.freeze({ moment: jieqiMoment, name: jieqiName });
export const lunar = Object.freeze({ supportedYearRange, yearInfo, fromGregorian, toGregorian });
export const chart = Object.freeze({ snapshot });
