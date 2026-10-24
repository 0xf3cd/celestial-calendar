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

import { BINDINGS, C_DECLARATIONS, CHART_CODES, LAYOUTS, PROTOCOLS } from "./bindings.mjs";
import {
  booleanValue,
  civilDateTime,
  civilDateTimeResult,
  enumValue,
  finiteNumber,
  gregorianDate,
  nativeCivilDateTime,
  nativeGregorianDate,
  rangedInteger,
  requiredRecord,
} from "./validation.mjs";

const WASM_URL = new URL("./celestial-jieqi.wasm", import.meta.url);
const UTF8 = new TextDecoder("utf-8", { fatal: true });
// Bound synchronous work and caller-owned WASM memory independently of native domains.
const MAX_NEW_MOON_COUNT = 4_096;
const MAX_OUTPUT_BYTES = 16 * 1024 * 1024;
const CODES = C_DECLARATIONS.constants;

const MOON_PHASE = Object.freeze({ new: 0, firstQuarter: 1, full: 2, lastQuarter: 3 });
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

const nativeInteger = (value, name, type = "int32_t") => {
  const unsigned = type.startsWith("uint");
  const width = Number(type.match(/\d+/)[0]);
  return rangedInteger(value, name, unsigned ? 0 : -(2 ** (width - 1)), 2 ** (unsigned ? width : width - 1) - 1);
};

const malformed = (operation, field) => {
  throw new CelestialError(operation, `Invalid native result: ${field}.`, false);
};

const nativeFinite = (value, operation, field) => {
  if (!Number.isFinite(value)) malformed(operation, field);
  return value;
};

const nativeCode = (value, prefix, operation, field) => {
  if (!Object.entries(CODES).some(([name, code]) => name.startsWith(prefix)
    && !name.endsWith("_DEFAULT") && code === value)) malformed(operation, field);
  return value;
};

const allocate = (M, bytes, operation) => {
  if (!Number.isSafeInteger(bytes) || bytes <= 0 || bytes > MAX_OUTPUT_BYTES) {
    throw new CelestialError(operation, `${operation} exceeds the binding's 16 MiB output limit.`, false);
  }
  const ptr = M._malloc(bytes);
  if (ptr === 0) {
    throw new CelestialError(operation, `${operation} failed to allocate the WASM output buffer.`, false);
  }
  return ptr;
};

const readField = (M, ptr, field) => {
  const address = ptr + field.offset;
  switch (field.type) {
    case "bool": return M.HEAPU8[address];
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

const checkError = (M, operation) => {
  const kind = M._celestial_last_error_kind() >>> 0;
  if (kind === CODES.CELESTIAL_ERROR_NONE) return;
  const message = M.ccall("celestial_last_error", "string", [], []);
  if (kind === CODES.CELESTIAL_ERROR_INVALID_INPUT) throw new RangeError(message || `${operation} rejected its input.`);
  nativeCode(kind, "CELESTIAL_ERROR_", operation, "error kind");
  throw new CelestialError(operation, message || `${operation} failed.`, message.length > 0);
};

const readCivilResult = (value, operation) => {
  try {
    return civilDateTimeResult(civilDateTime(value, "native civil result"));
  } catch {
    malformed(operation, "civil date/time");
  }
};

const callSret = (cName, operation, args) => {
  const M = requireModule(operation);
  const binding = bindingOf(cName);
  const layoutName = binding.result.slice("sret:".length);
  const layout = LAYOUTS[layoutName];
  const ptr = allocate(M, layout.size, operation);

  try {
    M[binding.wasmName](ptr, ...args);
    checkError(M, operation);
    if (readField(M, ptr, layout.fields.find((field) => field.name === "valid")) !== 1) {
      malformed(operation, "valid");
    }
    const value = readLayout(M, ptr, layoutName);
    for (const field of layout.fields) {
      if (field.type === "double") nativeFinite(value[field.name], operation, field.name);
      if (field.type === "bool" && value[field.name] !== 0 && value[field.name] !== 1) malformed(operation, field.name);
    }
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
  const protocol = PROTOCOLS?.[cName];
  const roles = ["count_parameter", "slots_parameter", "capacity_parameter"];
  const parameters = C_DECLARATIONS.functions?.[cName]?.parameters;
  if (protocol === null || typeof protocol !== "object" || Array.isArray(protocol)
    || ![...roles, "kind", "legal_empty", "total_must_match", "written_policy", "null_slots_requires_zero_capacity", "error_channel"]
      .every((name) => Object.hasOwn(protocol, name))
    || protocol.kind !== "count-fill" || binding.result !== protocol.kind
    || typeof protocol.legal_empty !== "boolean" || protocol.total_must_match !== true
    || protocol.written_policy !== "min-total-capacity" || protocol.error_channel !== "kind"
    || protocol.null_slots_requires_zero_capacity !== true || !Array.isArray(parameters)
    || !roles.every((role, index) => typeof protocol[role] === "string" && protocol[role].length > 0
      && protocol[role] === parameters.at(index - roles.length)?.name)) {
    malformed(operation, "count/fill protocol");
  }
  const countPtr = allocate(M, 4, operation);

  try {
    M.HEAPU32[countPtr >> 2] = 0;
    const queried = M[binding.wasmName](...args, countPtr, 0, 0);
    checkError(M, operation);
    const count = M.HEAPU32[countPtr >> 2];
    if (queried !== 0) malformed(operation, "count query wrote records");
    if (count === 0) {
      if (!protocol.legal_empty) malformed(operation, "unexpected empty result");
      return [];
    }

    const slots = allocate(M, count * 8, operation);
    try {
      M.HEAPF64.fill(NaN, slots >> 3, (slots >> 3) + count);
      const written = M[binding.wasmName](...args, countPtr, slots, count);
      checkError(M, operation);
      if (written !== count || M.HEAPU32[countPtr >> 2] !== count) {
        malformed(operation, "changed fill total");
      }
      return readDoubles(M, slots, count).map((value) => nativeFinite(value, operation, "moment"));
    } finally {
      M._free(slots);
    }
  } finally {
    M._free(countPtr);
  }
};

const ut1ToJd = (ut1) => {
  const operation = "time.ut1ToJd";
  requireModule(operation);
  const value = nativeCivilDateTime(ut1, "ut1");
  return callSret("celestial_ut1_to_jd", operation, [value.year, value.month, value.day, value.fraction]).value;
};

const ut1ToJde = (ut1) => {
  const operation = "time.ut1ToJde";
  requireModule(operation);
  const value = nativeCivilDateTime(ut1, "ut1");
  return callSret("celestial_ut1_to_jde", operation, [value.year, value.month, value.day, value.fraction]).value;
};

const jdeToUt1 = (jde) => {
  const operation = "time.jdeToUt1";
  requireModule(operation);
  const value = callSret("celestial_jde_to_ut1", operation, [finiteNumber(jde, "jde")]);
  return readCivilResult(value, operation);
};

const localApparentSiderealTime = (jdUt1, longitudeDeg) => {
  const operation = "time.localApparentSiderealTime";
  requireModule(operation);
  const jd = finiteNumber(jdUt1, "jdUt1");
  const longitude = finiteNumber(longitudeDeg, "longitudeDeg");
  const value = callSret("celestial_local_apparent_sidereal_time", operation, [jd, longitude]).value;
  if (value < 0 || value >= 360) malformed(operation, "sidereal angle");
  return value;
};

const deltaT = (year, model = "default") => {
  const operation = "time.deltaT";
  requireModule(operation);
  const decimalYear = finiteNumber(year, "year");
  const modelCode = enumValue(model, "model", CHART_CODES.models);
  const value = callSret("celestial_delta_t", operation, [decimalYear, modelCode]);
  nativeCode(value.model_identity_code, "CELESTIAL_DELTA_T_MODEL_", operation, "model_identity_code");
  if (modelCode !== CODES.CELESTIAL_DELTA_T_MODEL_DEFAULT && value.model_identity_code !== modelCode) {
    malformed(operation, "model_identity_code does not match selection");
  }
  return value.value;
};

const coordinate = (value, operation, distanceName) => {
  if (value.lon < 0 || value.lon >= 360 || value.lat < -90 || value.lat > 90 || value.r <= 0) {
    malformed(operation, "coordinate");
  }
  return { longitudeDeg: value.lon, latitudeDeg: value.lat, [distanceName]: value.r };
};

const sunApparentGeocentricCoordinate = (jde) => {
  const operation = "sun.apparentGeocentricCoordinate";
  requireModule(operation);
  const value = callSret("celestial_sun_apparent_geocentric_coord", operation, [finiteNumber(jde, "jde")]);
  return coordinate(value, operation, "distanceAu");
};

const longitudeCrossings = (year, longitudeDeg) => {
  const operation = "sun.longitudeCrossings";
  const M = requireModule(operation);
  const checkedYear = nativeInteger(year, "year");
  const longitude = finiteNumber(longitudeDeg, "longitudeDeg");
  const discriminant = callSret("celestial_solar_lon_root_discriminant", operation, [checkedYear, longitude]);
  if (discriminant.count > 2) malformed(operation, "root count");
  if (discriminant.count === 0) return [];

  const slots = allocate(M, discriminant.count * 8, operation);
  try {
    M.HEAPF64.fill(NaN, slots >> 3, (slots >> 3) + discriminant.count);
    const written = M._celestial_solar_lon_roots(checkedYear, longitude, slots, discriminant.count);
    checkError(M, operation);
    if (written !== discriminant.count) {
      malformed(operation, "root fill count");
    }
    return readDoubles(M, slots, written).map((value) => nativeFinite(value, operation, "root"));
  } finally {
    M._free(slots);
  }
};

const equationOfTime = (jde) => {
  const operation = "sun.equationOfTime";
  requireModule(operation);
  return callSret("celestial_equation_of_time", operation, [finiteNumber(jde, "jde")]).value;
};

const apparentSolarTime = (utc, longitudeDeg) => {
  const operation = "sun.apparentSolarTime";
  requireModule(operation);
  const value = nativeCivilDateTime(utc, "utc");
  const longitude = finiteNumber(longitudeDeg, "longitudeDeg");
  const result = callSret(
    "celestial_apparent_solar_time",
    operation,
    [value.year, value.month, value.day, value.fraction, longitude],
  );
  return readCivilResult(result, operation);
};

const moonApparentGeocentricCoordinate = (jde) => {
  const operation = "moon.apparentGeocentricCoordinate";
  requireModule(operation);
  const value = callSret("celestial_moon_apparent_geocentric_coord", operation, [finiteNumber(jde, "jde")]);
  return coordinate(value, operation, "distanceKm");
};

const illumination = (jde) => {
  const operation = "moon.illumination";
  requireModule(operation);
  const value = callSret("celestial_moon_illumination", operation, [finiteNumber(jde, "jde")]);
  if (value.illumination < 0 || value.illumination > 1 || value.elongation_deg < 0 || value.elongation_deg >= 360) {
    malformed(operation, "illumination");
  }
  return { fraction: value.illumination, elongationDeg: value.elongation_deg };
};

const brightLimbPositionAngle = (jde) => {
  const operation = "moon.brightLimbPositionAngle";
  requireModule(operation);
  const value = callSret("celestial_moon_position_angle", operation, [finiteNumber(jde, "jde")]).angle_deg;
  if (value < 0 || value >= 360) malformed(operation, "position angle");
  return value;
};

const phaseMoments = (year, phase) => {
  const operation = "moon.phaseMoments";
  requireModule(operation);
  const checkedYear = nativeInteger(year, "year");
  const phaseKind = enumValue(phase, "phase", MOON_PHASE);
  return countThenFill("celestial_moon_phase_moments", operation, [checkedYear, phaseKind]);
};

const newMoonsAfter = (jde, count) => {
  const operation = "moon.newMoonsAfter";
  const M = requireModule(operation);
  const start = finiteNumber(jde, "jde");
  const checkedCount = rangedInteger(count, "count", 0, MAX_NEW_MOON_COUNT);
  if (checkedCount === 0) return [];

  const slots = allocate(M, checkedCount * 8, operation);
  try {
    M.HEAPF64.fill(NaN, slots >> 3, (slots >> 3) + checkedCount);
    const written = M._celestial_new_moons_after_jde(start, slots, checkedCount);
    checkError(M, operation);
    if (written !== checkedCount) malformed(operation, "requested fill count");
    return readDoubles(M, slots, written).map((value) => nativeFinite(value, operation, "new moon"));
  } finally {
    M._free(slots);
  }
};

const newMoonsInYear = (year) => {
  const operation = "moon.newMoonsInYear";
  requireModule(operation);
  const checkedYear = nativeInteger(year, "year");
  return countThenFill("celestial_new_moons_in_year", operation, [checkedYear]);
};

const jieqiMoment = (year, index) => {
  const operation = "jieqi.moment";
  requireModule(operation);
  const checkedYear = nativeInteger(year, "year");
  const checkedIndex = nativeInteger(index, "index", "uint8_t");
  const value = callSret("celestial_query_jieqi_moment", operation, [checkedYear, checkedIndex]);
  if (value.jq_idx !== checkedIndex || !Object.values(Jieqi).includes(value.jq_idx)) malformed(operation, "jq_idx");
  return {
    jieqi: value.jq_idx,
    momentUt1: readCivilResult({ year: value.y, month: value.m, day: value.d, fraction: value.frac }, operation),
  };
};

const jieqiName = (index) => {
  const operation = "jieqi.name";
  const M = requireModule(operation);
  const checkedIndex = nativeInteger(index, "index", "uint8_t");
  const required = M._celestial_get_jieqi_name(checkedIndex, 0, 0) >>> 0;
  checkError(M, operation);
  if (required <= 1) malformed(operation, "required string size");
  const ptr = allocate(M, required, operation);

  try {
    M.HEAPU8.fill(0xFF, ptr, ptr + required);
    const writtenSize = M._celestial_get_jieqi_name(checkedIndex, ptr, required) >>> 0;
    checkError(M, operation);
    if (writtenSize !== required || M.HEAPU8[ptr + required - 1] !== 0
      || M.HEAPU8.subarray(ptr, ptr + required - 1).includes(0)) malformed(operation, "string fill/termination");
    try {
      return UTF8.decode(M.HEAPU8.slice(ptr, ptr + required - 1));
    } catch {
      malformed(operation, "UTF-8 string");
    }
  } finally {
    M._free(ptr);
  }
};

const supportedYearRange = (algorithm) => {
  const operation = "lunar.supportedYearRange";
  requireModule(operation);
  const nativeAlgorithm = enumValue(algorithm, "algorithm", LUNAR_ALGORITHM);
  const value = callSret("celestial_get_supported_lunar_year_range", operation, [nativeAlgorithm]);
  if (value.start > value.end) malformed(operation, "lunar year range");
  return { start: value.start, end: value.end };
};

const yearInfo = (algorithm, year) => {
  const operation = "lunar.yearInfo";
  requireModule(operation);
  const nativeAlgorithm = enumValue(algorithm, "algorithm", LUNAR_ALGORITHM);
  const checkedYear = nativeInteger(year, "year");
  const value = callSret("celestial_get_lunar_year_info", operation, [nativeAlgorithm, checkedYear]);
  if (value.leap_month > 12) malformed(operation, "leap_month");
  try { gregorianDate({ year: value.year, month: value.month, day: value.day }, "native first day"); }
  catch { malformed(operation, "lunar first day"); }
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
  const value = nativeGregorianDate(date, "date");
  const result = callSret(
    "celestial_gregorian_to_lunar",
    operation,
    [nativeAlgorithm, value.year, value.month, value.day],
  );
  if (result.month < 1 || result.month > 12 || result.day < 1 || result.day > 30) malformed(operation, "lunar date");
  return { year: result.year, month: result.month, day: result.day, isLeap: result.is_leap === 1 };
};

const toGregorian = (algorithm, date) => {
  const operation = "lunar.toGregorian";
  requireModule(operation);
  const nativeAlgorithm = enumValue(algorithm, "algorithm", LUNAR_ALGORITHM);
  requiredRecord(date, "date", ["year", "month", "day", "isLeap"], ["fraction"]);
  const year = nativeInteger(date.year, "date.year");
  const month = nativeInteger(date.month, "date.month", "uint8_t");
  const day = nativeInteger(date.day, "date.day", "uint8_t");
  const isLeap = booleanValue(date.isLeap, "date.isLeap");
  const value = callSret("celestial_lunar_to_gregorian", operation, [nativeAlgorithm, year, month, isLeap, day]);
  try { return gregorianDate({ year: value.year, month: value.month, day: value.day }, "native date"); }
  catch { malformed(operation, "Gregorian date"); }
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
  const readSnapshot = (name) => readChartField(M, ptr, "CelestialChartSnapshot", name);
  nativeCode(readSnapshot("delta_t_identity_code"), "CELESTIAL_DELTA_T_MODEL_", "chart.snapshot", "delta_t_identity_code");
  nativeCode(readSnapshot("nutation_identity_code"), "CELESTIAL_NUTATION_MODEL_", "chart.snapshot", "nutation_identity_code");
  const bodyField = chartField("CelestialChartSnapshot", "bodies");
  const targetCount = Object.keys(CHART_CODES.targets).length;
  if (Number(bodyField.type.match(/\[(\d+)\]$/)[1]) !== targetCount) malformedChart("body count");
  const bodiesPtr = ptr + bodyField.offset;
  const bodies = Object.entries(CHART_CODES.targets).map(([target, code], index) => {
    const bodyPtr = bodiesPtr + index * LAYOUTS.CelestialChartBody.size;
    const readBody = (name) => readChartField(M, bodyPtr, "CelestialChartBody", name);
    const isNode = code >= CHART_CODES.targets.meanAscending;
    const present = CHART_CODES.present.latitude | (isNode ? 0 : CHART_CODES.present.distance);
    if (readBody("target_code") !== code) malformedChart(`bodies[${index}].target_code`);
    nativeCode(readBody("source_code"), "CELESTIAL_POSITION_SOURCE_", "chart.snapshot", `${target}.source_code`);
    if (readBody("present_fields") !== present) malformedChart(`${target}.present_fields`);
    const latitudeDeg = chartFinite(readBody("latitude_deg"), `${target}.latitude_deg`);
    if (latitudeDeg < -90 || latitudeDeg > 90 || (isNode && latitudeDeg !== 0)) {
      malformedChart(`${target}.latitude_deg`);
    }
    // Node distance storage is absent, not a numeric sentinel.
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

  const housesPtr = ptr + chartField("CelestialChartSnapshot", "houses").offset;
  const houseAngle = (name) => chartLongitude(
    readChartField(M, housesPtr, "CelestialChartHouses", name),
    `houses.${name}`,
  );
  const cuspField = chartField("CelestialChartHouses", "cusps_deg");
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
  const value = nativeCivilDateTime(civil, "civil");
  requiredRecord(options, "options", ["scale", "location", "system"], []);
  if (Reflect.ownKeys(options).some((key) => !["scale", "location", "system", "model"].includes(key))) {
    throw new TypeError("options must contain only: scale, location, system, model.");
  }
  const hasModel = Object.hasOwn(options, "model");
  if (!hasModel && "model" in options) {
    throw new TypeError("options.model must be an own property.");
  }
  const scale = enumValue(options.scale, "options.scale", CHART_CODES.scales);
  const location = requiredRecord(
    options.location,
    "options.location",
    ["latitudeDeg", "longitudeDeg"],
    [],
  );
  const latitude = finiteNumber(location.latitudeDeg, "options.location.latitudeDeg");
  const longitude = finiteNumber(location.longitudeDeg, "options.location.longitudeDeg");
  const system = enumValue(options.system, "options.system", CHART_CODES.systems);
  const model = hasModel ? options.model : undefined;
  const modelCode = enumValue(model === undefined ? "default" : model, "options.model", CHART_CODES.models);

  const binding = bindingOf("celestial_chart_snapshot");
  const ptr = allocate(M, LAYOUTS.CelestialChartSnapshot.size, operation);
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
    checkError(M, operation);
    const valid = M.HEAPU8[ptr + chartField("CelestialChartSnapshot", "valid").offset];
    if (valid !== 1) malformedChart("valid");
    if (modelCode !== CODES.CELESTIAL_DELTA_T_MODEL_DEFAULT
      && readChartField(M, ptr, "CelestialChartSnapshot", "delta_t_identity_code") !== modelCode) {
      malformedChart("delta_t_identity_code does not match selection");
    }
    return readChartSnapshot(M, ptr);
  } finally {
    M._free(ptr);
  }
};

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
