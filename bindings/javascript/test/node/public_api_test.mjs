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
import { readFile, rename } from "node:fs/promises";
import { resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { API, SymbolFlags } from "typescript/unstable/sync";
import {
  isLiteralTypeNode, isNumericLiteral, isStringLiteral, isTypeLiteralNode, isTypeReferenceNode, isUnionTypeNode,
} from "typescript/unstable/ast";
import { testChartApi } from "../support/chart_api.mjs";
import { chartLabel } from "../support/chart_replay.mjs";
import { testFakeNative } from "../support/fake_native.mjs";

if (process.argv.length !== 3) throw new Error("usage: node public_api_test.mjs <staged index.mjs>");

const entryUrl = pathToFileURL(resolve(process.argv[2]));
const wasmPath = fileURLToPath(new URL("./celestial-jieqi.wasm", entryUrl));
const heldWasmPath = `${wasmPath}.init-failure-probe`;
const celestial = await import(entryUrl);
const declarationPath = fileURLToPath(new URL("./index.d.ts", entryUrl));
const declarationText = await readFile(declarationPath, "utf8");
const namespaces = ["time", "sun", "moon", "jieqi", "lunar", "chart"];
const { C_DECLARATIONS, CHART_CODES } = await import(new URL("./bindings.mjs", entryUrl));
const manifest = JSON.parse(await readFile(new URL("../abi/manifest.json", import.meta.url), "utf8"));
assert.equal(manifest.schema, "celestial-calendar/javascript-wasm-abi@2");
assert.deepEqual(manifest.codes, C_DECLARATIONS.constants, "staged declarations contain every named code");
const expectedTargetCodes = Object.entries(manifest.codes)
  .filter(([name]) => name.startsWith("CELESTIAL_TARGET_") && name !== "CELESTIAL_TARGET_ALL")
  .sort((left, right) => left[1] - right[1])
  .map(([name, value]) => [chartLabel(name.slice("CELESTIAL_TARGET_".length)), value]);
assert.deepEqual(Object.entries(CHART_CODES.targets), expectedTargetCodes, "staged target labels and full-width codes");
const expectedChartTargets = expectedTargetCodes.map(([name]) => name);

const declaredSurface = (text) => {
  const api = new API({ fs: { readFile: (path) => resolve(path) === declarationPath ? text : undefined } });
  try {
    const snapshot = api.updateSnapshot({ openFiles: [declarationPath] });
    const { program, checker } = snapshot.getDefaultProjectForFile(declarationPath);
    assert.deepEqual(program.getSyntacticDiagnostics(declarationPath), [], "parse staged index.d.ts");
    const source = program.getSourceFile(declarationPath);
    const exports = checker.getExportsOfModule(checker.getSymbolAtLocation(source));
    assert(!exports.some((symbol) => symbol.flags & SymbolFlags.Alias), "re-exports need explicit value/type handling");
    const values = exports.filter((symbol) => symbol.flags & SymbolFlags.Value);
    const members = {};
    let jieqi;
    for (const name of [...namespaces, "Jieqi"]) {
      const symbol = values.find((value) => value.name === name);
      assert(symbol, `declared value ${name}`);
      const type = symbol.valueDeclaration.resolve().type;
      assert(isTypeReferenceNode(type) && type.typeName.text === "Readonly", `${name}: Readonly declaration`);
      assert.equal(type.typeArguments.length, 1);
      const record = type.typeArguments[0];
      assert(isTypeLiteralNode(record), `${name}: declared member block`);
      members[name] = Array.from(record.members, (member) => member.name.text).sort();
      if (name === "Jieqi") {
        jieqi = Object.fromEntries(Array.from(record.members, (member) => {
          assert(isLiteralTypeNode(member.type) && isNumericLiteral(member.type.literal), "Jieqi numeric literals");
          return [member.name.text, Number(member.type.literal.text)];
        }));
      }
    }
    const chartTarget = exports.find((symbol) => symbol.name === "ChartTarget");
    assert(chartTarget, "ChartTarget declaration");
    const targetUnion = chartTarget.declarations[0].resolve().type;
    assert(isUnionTypeNode(targetUnion), "ChartTarget union");
    const chartTargets = Array.from(targetUnion.types, (type) => {
      assert(isLiteralTypeNode(type) && isStringLiteral(type.literal), "ChartTarget string literals");
      return type.literal.text;
    });
    return { values: values.map((symbol) => symbol.name).sort(), members, jieqi, chartTargets };
  } finally {
    api.close();
  }
};
const assertSurface = (runtime, declared) => {
  assert.deepEqual(Object.keys(runtime).sort(), declared.values, "runtime/staged declaration value exports");
  for (const name of namespaces) {
    assert.deepEqual(Object.keys(runtime[name]).sort(), declared.members[name], `${name} runtime/declaration members`);
  }
  assert.deepEqual(declared.chartTargets, expectedChartTargets, "header-derived target declaration order");
};
const declared = declaredSurface(declarationText);
assertSurface(celestial, declared);

assert.throws(() => assertSurface({ ...celestial, runtimeOnly: true }, declared), assert.AssertionError);
assert.throws(
  () => assertSurface(celestial, declaredSurface(`${declarationText}\nexport const declarationOnly: boolean;\n`)),
  { code: "ERR_ASSERTION", message: /^runtime\/staged declaration value exports/ },
);
assert.throws(
  () => assertSurface({ ...celestial, sun: { ...celestial.sun, runtimeOnly() {} } }, declared),
  { code: "ERR_ASSERTION", message: /^sun runtime\/declaration members/ },
);
const extraMember = declarationText.replace("export const sun: Readonly<{", "export const sun: Readonly<{ declarationOnly(): void;");
assert.notEqual(extraMember, declarationText, "namespace drift mutation applied");
assert.throws(
  () => assertSurface(celestial, declaredSurface(extraMember)),
  { code: "ERR_ASSERTION", message: /^sun runtime\/declaration members/ },
);
const swappedTargets = declarationText.replace('"mercury" | "venus"', '"venus" | "mercury"');
assert.notEqual(swappedTargets, declarationText, "target order mutation applied");
assert.throws(() => assertSurface(celestial, declaredSurface(swappedTargets)), assert.AssertionError);
console.log("PASS staged runtime/declaration surface; drift mutations 5/5");

const expectedJieqi = {
  LICHUN: 0, YUSHUI: 1, JINGZHE: 2, CHUNFEN: 3, QINGMING: 4, GUYU: 5,
  LIXIA: 6, XIAOMAN: 7, MANGZHONG: 8, XIAZHI: 9, XIAOSHU: 10, DASHU: 11,
  LIQIU: 12, CHUSHU: 13, BAILU: 14, QIUFEN: 15, HANLU: 16, SHUANGJIANG: 17,
  LIDONG: 18, XIAOXUE: 19, DAXUE: 20, DONGZHI: 21, XIAOHAN: 22, DAHAN: 23,
};
assert.equal(Object.keys(expectedJieqi).length, 24);
assert.deepEqual(celestial.Jieqi, expectedJieqi, "exact Jieqi runtime keys and values");
assert.deepEqual(declared.jieqi, expectedJieqi, "exact Jieqi declaration keys and literal types");
assert(Object.isFrozen(celestial.Jieqi), "Jieqi is frozen");

assert.deepEqual(
  Object.keys(celestial).sort(),
  ["CelestialError", "Jieqi", "chart", "init", "jieqi", "lunar", "moon", "sun", "time"],
  "public module exports",
);
assert.equal("config" in celestial, false);
assert.deepEqual(
  Object.keys(celestial.time),
  ["ut1ToJd", "ut1ToJde", "jdeToUt1", "localApparentSiderealTime", "deltaT"],
);
assert.deepEqual(
  Object.keys(celestial.sun),
  ["apparentGeocentricCoordinate", "longitudeCrossings", "equationOfTime", "apparentSolarTime"],
);
assert.deepEqual(
  Object.keys(celestial.moon),
  [
    "apparentGeocentricCoordinate",
    "illumination",
    "brightLimbPositionAngle",
    "phaseMoments",
    "newMoonsAfter",
    "newMoonsInYear",
  ],
);
assert.deepEqual(Object.keys(celestial.jieqi), ["moment", "name"]);
assert.deepEqual(Object.keys(celestial.lunar), ["supportedYearRange", "yearInfo", "fromGregorian", "toGregorian"]);
assert.deepEqual(Object.keys(celestial.chart), ["snapshot"]);

let edges = 0;
const edge = (label, action, ErrorType) => {
  assert.throws(action, ErrorType, label);
  ++edges;
};

const uninitializedCalls = [
  ["chart.snapshot", [{ year: 2026, month: 1, day: 1, fraction: 0.5 }, { scale: "utc", location: { latitudeDeg: 51.5, longitudeDeg: 0 }, system: "placidus" }]],
  ["time.ut1ToJd", [{ year: 2000, month: 1, day: 1, fraction: 0.5 }]],
  ["time.ut1ToJde", [{ year: 2000, month: 1, day: 1, fraction: 0.5 }]],
  ["time.jdeToUt1", [2451545.0]],
  ["time.localApparentSiderealTime", [2451545.0, 0]],
  ["time.deltaT", [2024.5]],
  ["sun.apparentGeocentricCoordinate", [2451545.0]],
  ["sun.longitudeCrossings", [2024, 0]],
  ["sun.equationOfTime", [2451545.0]],
  ["sun.apparentSolarTime", [{ year: 2024, month: 6, day: 1, fraction: 0.5 }, 0]],
  ["moon.apparentGeocentricCoordinate", [2451545.0]],
  ["moon.illumination", [2448724.5]],
  ["moon.brightLimbPositionAngle", [2448724.5]],
  ["moon.phaseMoments", [2024, "new"]],
  ["moon.newMoonsAfter", [2451545.0, 1]],
  ["moon.newMoonsInYear", [2024]],
  ["jieqi.moment", [2024, 0]],
  ["jieqi.name", [0]],
  ["lunar.supportedYearRange", ["algo3"]],
  ["lunar.yearInfo", ["algo3", 2024]],
  ["lunar.fromGregorian", ["algo3", { year: 2024, month: 2, day: 10 }]],
  ["lunar.toGregorian", ["algo3", { year: 2024, month: 1, day: 1, isLeap: false }]],
];
assert.deepEqual(
  uninitializedCalls.map(([operation]) => operation).sort(),
  namespaces.flatMap((name) => declared.members[name].map((method) => `${name}.${method}`)).sort(),
  "pre-init cases cover every namespace method",
);
const checkUninitialized = (runtime, operation, args) => {
  const [namespace, method] = operation.split(".");
  assert.throws(() => runtime[namespace][method](...args), (error) => {
    assert(error instanceof runtime.CelestialError, `${operation}: pre-init CelestialError identity`);
    assert.equal(error.name, "CelestialError");
    assert.equal(error.operation, operation);
    assert.equal(error.message, `Call and await init() before ${operation}().`);
    assert.equal(error.recorded, false);
    return true;
  });
};
for (const [operation, args] of uninitializedCalls) {
  checkUninitialized(celestial, operation, args);
  checkUninitialized(celestial, operation, []);
}
checkUninitialized(celestial, "moon.newMoonsAfter", [2451545.0, 0]);
checkUninitialized(celestial, "moon.newMoonsAfter", [NaN, 0]);
console.log(`PASS pre-init coverage: ${uninitializedCalls.length} methods; validation order; zero count`);

await rename(wasmPath, heldWasmPath);
let failedInitialization;
const initializationErrors = [];
const originalConsoleError = console.error;
try {
  console.error = (...args) => initializationErrors.push(args.join(" "));
  failedInitialization = celestial.init();
  assert.strictEqual(celestial.init(), failedInitialization, "concurrent init shares one promise");
  checkUninitialized(celestial, "moon.illumination", [2448724.5]);
  await assert.rejects(failedInitialization);
  checkUninitialized(celestial, "moon.illumination", [2448724.5]);
} finally {
  console.error = originalConsoleError;
  await rename(heldWasmPath, wasmPath);
}
assert(initializationErrors.length > 0, "failed init reported its load error");

const retry = celestial.init();
assert.notStrictEqual(retry, failedInitialization, "failed init can retry");
assert.equal(await retry, undefined, "init resolves without a namespace");
assert.strictEqual(celestial.init(), retry, "completed init reuses one promise");

// English aliases and Chinese names follow calendar/jieqi.hpp, starting at Lichun.
const jieqiNames = [
  "立春", "雨水", "惊蛰", "春分", "清明", "谷雨", "立夏", "小满", "芒种", "夏至", "小暑", "大暑",
  "立秋", "处暑", "白露", "秋分", "寒露", "霜降", "立冬", "小雪", "大雪", "冬至", "小寒", "大寒",
];
const golden = JSON.parse(await readFile(new URL("../../../../toolbox/bindings_golden.json", import.meta.url), "utf8"));
const bitsOf = (hex) => Buffer.from(hex.slice(2), "hex").readDoubleBE(0);
assert.equal(jieqiNames.length, 24);
for (const [name, index] of Object.entries(expectedJieqi)) {
  const jq = celestial.Jieqi[name];
  assert.equal(celestial.jieqi.name(jq), jieqiNames[index], name);
  const point = golden.sections.jieqi.entries.find((entry) => entry.idx === index);
  assert(point, `missing Jieqi reference for ${name}`);
  const value = celestial.jieqi.moment(point.year, jq);
  const moment = value.momentUt1;
  assert.deepEqual([value.jieqi, moment.year, moment.month, moment.day], [point.idx, point.y, point.m, point.d]);
  // Same native-output reference and WASM/libm moment cap as test/abi/raw_protocol_test.mjs.
  assert(Math.abs(moment.fraction - bitsOf(point.frac_bits)) <= 1e-8, `${name}: reference UT1 fraction`);
}
// Delegated/fitted/extrapolated cases from src/test/astro/delta_t_test.cpp::DefaultDispatch.
for (const year of [1950, 2020, 2040]) {
  const expected = celestial.time.deltaT(year, "algo5");
  assert.equal(celestial.time.deltaT(year), expected, `${year}: omitted model`);
  assert.equal(celestial.time.deltaT(year, "default"), expected, `${year}: default model`);
}
console.log("PASS public Jieqi names/moments 24/24; default/algo5 equivalence");

let happy = 0;
const check = (label, action) => {
  action();
  ++happy;
  console.log(`PASS public ${label}`);
};
const finite = (...values) => values.forEach((value) => assert(Number.isFinite(value)));
const civilResult = (value) => {
  assert.deepEqual(Object.keys(value).sort(), ["day", "fraction", "hour", "minute", "month", "second", "year"]);
  assert(Number.isInteger(value.hour) && value.hour >= 0 && value.hour < 24, "integer hour in [0, 24)");
  assert(Number.isInteger(value.minute) && value.minute >= 0 && value.minute < 60, "integer minute in [0, 60)");
  assert(Number.isFinite(value.second) && value.second >= 0 && value.second < 60, "fractional second in [0, 60)");
  const seconds = value.fraction * 86400;
  assert.equal(value.hour, Math.floor(seconds / 3600));
  assert.equal(value.minute, Math.floor((seconds - value.hour * 3600) / 60));
  assert.equal(value.second, seconds - value.hour * 3600 - value.minute * 60, "second retains the remainder");
};

check("time.ut1ToJd", () => assert.equal(
  celestial.time.ut1ToJd({ year: 2000, month: 1, day: 1, fraction: 0.5 }),
  2451545.0,
));
check("time.ut1ToJde", () => finite(
  celestial.time.ut1ToJde({ year: 2024, month: 6, day: 1, fraction: 0.5 }),
));
check("time.jdeToUt1", () => {
  const value = celestial.time.jdeToUt1(2451545.0);
  assert.deepEqual([value.year, value.month, value.day], [2000, 1, 1]);
  finite(value.fraction);
  civilResult(value);
  assert(!Number.isInteger(value.second), "JDE-derived civil output retains subsecond precision");
});
check("time.localApparentSiderealTime", () => {
  const value = celestial.time.localApparentSiderealTime(2451545.0, 0);
  assert(value >= 0 && value < 360);
});
check("time.deltaT", () => {
  for (const model of ["default", "algo1", "algo2", "algo3", "algo4", "algo5"]) {
    finite(celestial.time.deltaT(2024.5, model));
  }
});
check("sun.apparentGeocentricCoordinate", () => {
  const value = celestial.sun.apparentGeocentricCoordinate(2451545.0);
  finite(value.longitudeDeg, value.latitudeDeg, value.distanceAu);
  assert.equal("radiusAu" in value, false);
});
check("sun.longitudeCrossings", () => {
  assert.equal(celestial.sun.longitudeCrossings(2024, 0).length, 1);
  const roots = celestial.sun.longitudeCrossings(2024, 280.1);
  assert.equal(roots.length, 2);
  assert(roots[0] < roots[1]);
});
check("sun.equationOfTime", () => finite(celestial.sun.equationOfTime(2451545.0)));
check("sun.apparentSolarTime", () => {
  const value = celestial.sun.apparentSolarTime({ year: 2024, month: 6, day: 1, fraction: 0.5 }, 116.4);
  assert.deepEqual([value.year, value.month, value.day], [2024, 6, 1]);
  finite(value.fraction);
  civilResult(value);
});
check("moon.apparentGeocentricCoordinate", () => {
  const value = celestial.moon.apparentGeocentricCoordinate(2451545.0);
  finite(value.longitudeDeg, value.latitudeDeg, value.distanceKm);
});
check("moon.illumination", () => {
  const value = celestial.moon.illumination(2448724.5);
  assert(Math.abs(value.fraction - 0.6786) < 5e-5);
  finite(value.elongationDeg);
});
check("moon.brightLimbPositionAngle", () => finite(celestial.moon.brightLimbPositionAngle(2448724.5)));
check("moon.phaseMoments", () => {
  for (const phase of ["new", "firstQuarter", "full", "lastQuarter"]) {
    assert(celestial.moon.phaseMoments(2024, phase).length >= 12);
  }
});
check("moon.newMoonsAfter", () => assert.equal(celestial.moon.newMoonsAfter(2451545.0, 3).length, 3));
check("moon.newMoonsInYear", () => assert(celestial.moon.newMoonsInYear(2024).length >= 12));
check("jieqi.moment", () => {
  const value = celestial.jieqi.moment(401, celestial.Jieqi.LICHUN);
  assert.deepEqual(Object.keys(value).sort(), ["jieqi", "momentUt1"], "no former flat Jieqi fields");
  assert.equal(value.jieqi, celestial.Jieqi.LICHUN);
  assert.deepEqual([value.momentUt1.year, value.momentUt1.month, value.momentUt1.day], [401, 2, 3]);
  civilResult(value.momentUt1);
});
check("jieqi.name", () => assert.equal(celestial.jieqi.name(0), "立春"));
check("lunar.supportedYearRange", () => {
  assert.deepEqual(celestial.lunar.supportedYearRange("algo1"), { start: 1901, end: 2099 });
  assert.deepEqual(celestial.lunar.supportedYearRange("algo2"), { start: 410, end: 2500 });
  assert.deepEqual(celestial.lunar.supportedYearRange("algo3"), { start: 1600, end: 2199 });
});
// The HKO-backed 2024-02-10 new-year anchor is sourced in lunar/common_test.cpp::ParseLunarYear.
check("lunar.yearInfo", () => {
  const value = celestial.lunar.yearInfo("algo3", 2024);
  assert.deepEqual(value.firstDay, { year: 2024, month: 2, day: 10 });
  assert.equal(value.leapMonth, null);
  assert.deepEqual(value.monthLengths, [29, 30, 29, 29, 30, 29, 30, 30, 29, 30, 30, 29]);

  const algo2 = celestial.lunar.yearInfo("algo2", 2024);
  assert.deepEqual(algo2.firstDay, { year: 2024, month: 2, day: 10 });

  // Same HKO-backed leap-year anchor as lunar/common_test.cpp.
  const leap = celestial.lunar.yearInfo("algo1", 2023);
  assert.equal(leap.leapMonth, 2);
  assert.deepEqual(leap.monthLengths, [29, 30, 29, 29, 30, 30, 29, 30, 30, 29, 30, 29, 30]);
});
check("lunar.fromGregorian", () => {
  for (const algorithm of ["algo2", "algo3"]) {
    assert.deepEqual(
      celestial.lunar.fromGregorian(algorithm, { year: 2024, month: 2, day: 10 }),
      { year: 2024, month: 1, day: 1, isLeap: false },
    );
  }
});
check("lunar.toGregorian", () => {
  for (const algorithm of ["algo2", "algo3"]) {
    assert.deepEqual(
      celestial.lunar.toGregorian(algorithm, { year: 2024, month: 1, day: 1, isLeap: false }),
      { year: 2024, month: 2, day: 10 },
    );
  }
});

{
  const lichun = celestial.jieqi.moment(2024, 0);
  for (const convert of [celestial.time.ut1ToJd, celestial.time.ut1ToJde, (value) => celestial.sun.apparentSolarTime(value, 116.4)]) {
    assert.throws(() => convert(lichun), TypeError, "Jieqi result is not a civil moment");
    assert.doesNotThrow(() => convert(lichun.momentUt1), "nested rich civil output remains a civil input");
  }

  const ut1 = celestial.time.jdeToUt1(2451545.0);
  const date = { year: ut1.year, month: ut1.month, day: ut1.day };
  const fromMoment = celestial.lunar.fromGregorian("algo3", date);
  assert.equal(fromMoment.year, 1999);
  for (const value of [lichun, lichun.momentUt1, ut1, fromMoment]) {
    assert.throws(() => celestial.lunar.fromGregorian("algo3", value), TypeError, "cross-kind Gregorian input");
  }
  assert.deepEqual(
    celestial.lunar.fromGregorian(
      "algo3",
      celestial.lunar.toGregorian("algo3", { ...fromMoment, source: "public output" }),
    ),
    fromMoment,
  );
  console.log("PASS explicit date extraction; cross-kind records rejected; unrelated properties accepted");
}

const gregorian = { year: 2024, month: 2, day: 10 };
const civil = { ...gregorian, fraction: 0 };
const lunarDate = { year: 2024, month: 1, day: 1, isLeap: false };
const semanticExclusions = [
  ["Gregorian fraction", (value) => celestial.lunar.fromGregorian("algo3", value), gregorian, "fraction", 0],
  ["Gregorian isLeap", (value) => celestial.lunar.fromGregorian("algo3", value), gregorian, "isLeap", false],
  ["UT1/JD isLeap", celestial.time.ut1ToJd, civil, "isLeap", false],
  ["UT1/JDE isLeap", celestial.time.ut1ToJde, civil, "isLeap", false],
  ["solar civil isLeap", (value) => celestial.sun.apparentSolarTime(value, 0), civil, "isLeap", false],
  ["lunar fraction", (value) => celestial.lunar.toGregorian("algo3", value), lunarDate, "fraction", 0],
];
let excluded = 0;
for (const [label, convert, record, property, value] of semanticExclusions) {
  assert.deepEqual(convert({ ...record, source: "extra field" }), convert(record), `${label}: unrelated field accepted`);
  for (const tag of [value, undefined]) {
    for (const inherited of [false, true]) {
      const input = inherited
        ? Object.assign(Object.create({ [property]: tag }), record)
        : { ...record, [property]: tag };
      assert.throws(() => convert(input), TypeError, `${label}: ${inherited ? "inherited" : "own"} ${tag}`);
      ++excluded;
    }
  }
}
assert.equal(excluded, 24);
console.log(`PASS semantic property exclusions ${excluded}/24`);

for (const [convert, fields] of [
  [(value) => celestial.lunar.fromGregorian("algo3", value), gregorian],
  [(value) => celestial.lunar.toGregorian("algo3", value), lunarDate],
  [celestial.time.ut1ToJd, civil],
  [celestial.time.ut1ToJde, civil],
  [(value) => celestial.sun.apparentSolarTime(value, 0), civil],
]) {
  const expected = convert(fields);
  assert.throws(() => convert(new Date(0)), TypeError, "ordinary Date lacks record fields");
  for (const input of [new Date(0), new Date(NaN), Object.create(null)]) {
    Object.assign(input, fields, {
      getTime: () => assert.fail("record conversion must not read a Date timestamp"),
    });
    assert.deepEqual(convert(input), expected, "explicit fields determine the record value");
  }
}
console.log("PASS structural date records; Date timestamps ignored");

// Keep boundary categories aligned with bindings/python/test/consumer/smoke.py::run_acceptance_boundaries;
// each package runner remains independent.
const acceptedBoundaries = [
  ["civil year lower", () => finite(celestial.time.ut1ToJd({ year: 1, month: 1, day: 1, fraction: 0 }))],
  ["civil year upper", () => finite(celestial.time.ut1ToJd({ year: 32767, month: 1, day: 1, fraction: 0 }))],
  ["civil fraction lower", () => assert.equal(
    celestial.time.ut1ToJd({ year: 2000, month: 1, day: 1, fraction: 0 }),
    2451544.5,
  )],
  ["phase year upper", () => assert(celestial.moon.phaseMoments(32766, "new").length >= 12)],
  ["new moons year upper", () => assert(celestial.moon.newMoonsInYear(32766).length >= 12)],
  ["Jieqi year lower", () => assert.equal(celestial.jieqi.moment(401, 0).momentUt1.year, 401)],
  ["Jieqi year upper", () => assert.equal(celestial.jieqi.moment(32766, 0).momentUt1.year, 32766)],
  ["longitude lower", () => finite(celestial.time.localApparentSiderealTime(2451545.0, -180))],
  ["longitude upper", () => finite(celestial.time.localApparentSiderealTime(2451545.0, 180))],
  ["solar longitude lower", () => finite(celestial.sun.apparentSolarTime(civil, -180).fraction)],
  ["solar longitude upper", () => finite(celestial.sun.apparentSolarTime(civil, 180).fraction)],
  ["delta T algo1 lower", () => finite(celestial.time.deltaT(-4000, "algo1"))],
  ["algo1 year lower", () => assert.equal(celestial.lunar.yearInfo("algo1", 1901).firstDay.year, 1901)],
  ["algo1 year upper", () => assert.equal(celestial.lunar.yearInfo("algo1", 2099).firstDay.year, 2099)],
  ["algo2 year lower", () => assert.equal(celestial.lunar.yearInfo("algo2", 410).firstDay.year, 410)],
  ["algo2 year upper", () => assert.equal(celestial.lunar.yearInfo("algo2", 2500).firstDay.year, 2500)],
  ["algo3 year lower", () => assert.equal(celestial.lunar.yearInfo("algo3", 1600).firstDay.year, 1600)],
  ["algo3 year upper", () => assert.equal(celestial.lunar.yearInfo("algo3", 2199).firstDay.year, 2199)],
];
for (const [label, action] of acceptedBoundaries) assert.doesNotThrow(action, label);

assert.deepEqual(celestial.sun.longitudeCrossings(1, 281.3), [], "valid no-root result");
assert.deepEqual(celestial.moon.newMoonsAfter(2451545.0, 0), [], "zero requested count");

edge("date object", () => celestial.time.ut1ToJd(null), TypeError);
edge("missing date field", () => celestial.time.ut1ToJd({ year: 2024, month: 1, day: 1 }), TypeError);
assert.doesNotThrow(
  () => celestial.time.ut1ToJd({ year: 2024, month: 1, day: 1, fraction: 0, utc: true }),
  "additional date field",
);
edge(
  "real Gregorian date",
  () => celestial.time.ut1ToJd({ year: 2023, month: 2, day: 29, fraction: 0 }),
  RangeError,
);
edge("fraction range", () => celestial.time.ut1ToJd({ year: 2024, month: 1, day: 1, fraction: 1 }), RangeError);
edge("civil year range", () => celestial.time.ut1ToJd({ year: 0, month: 1, day: 1, fraction: 0 }), RangeError);
edge("number type", () => celestial.time.jdeToUt1("1"), TypeError);
edge("finite number", () => celestial.time.jdeToUt1(Number.NaN), RangeError);
edge("geographic longitude", () => celestial.time.localApparentSiderealTime(2451545.0, 181), RangeError);
edge("solar geographic longitude", () => celestial.sun.apparentSolarTime(civil, 181), RangeError);
edge("finite delta T year", () => celestial.time.deltaT(Number.POSITIVE_INFINITY), RangeError);
edge("delta T model type", () => celestial.time.deltaT(2024, 5), TypeError);
edge("delta T model value", () => celestial.time.deltaT(2024, "future"), RangeError);
edge("algo1 domain", () => celestial.time.deltaT(-4001, "algo1"), RangeError);
edge("algo3 domain", () => celestial.time.deltaT(3000, "algo3"), RangeError);
edge("algo4 domain", () => celestial.time.deltaT(2035, "algo4"), RangeError);
edge("solar year domain", () => celestial.sun.longitudeCrossings(0, 0), RangeError);
edge("solar longitude domain", () => celestial.sun.longitudeCrossings(2024, 360), RangeError);
edge("phase type", () => celestial.moon.phaseMoments(2024, 0), TypeError);
edge("count boolean", () => celestial.moon.newMoonsAfter(2451545.0, true), TypeError);
edge("count integer", () => celestial.moon.newMoonsAfter(2451545.0, 1.5), TypeError);
edge("count non-negative", () => celestial.moon.newMoonsAfter(2451545.0, -1), RangeError);
edge("count resource bound", () => celestial.moon.newMoonsAfter(2451545.0, 4097), RangeError);
edge("Jieqi year", () => celestial.jieqi.moment(400, 0), RangeError);
edge("Jieqi index", () => celestial.jieqi.name(24), RangeError);
edge("lunar algorithm", () => celestial.lunar.yearInfo("algo4", 2024), RangeError);
edge(
  "lunar boolean",
  () => celestial.lunar.toGregorian("algo3", { year: 2024, month: 1, day: 1, isLeap: null }),
  TypeError,
);

let recordingError;
try {
  celestial.time.localApparentSiderealTime(1000000.0, 0);
} catch (error) {
  recordingError = error;
}
assert(recordingError instanceof RangeError);
++edges;

let lunarError;
try {
  celestial.lunar.fromGregorian("algo1", { year: 1900, month: 1, day: 1 });
} catch (error) {
  lunarError = error;
}
assert(lunarError instanceof RangeError);
assert.match(lunarError.message, /cannot be represented/);
++edges;

assert.equal(celestial.jieqi.name(0), "立春", "module survives translated errors");
happy += await testChartApi(celestial, entryUrl);
assert.equal(happy, 22, "public method denominator");
assert.equal(edges, 28, "public edge denominator");
assert.equal(acceptedBoundaries.length, 18, "public acceptance denominator");
console.log(`PASS public methods ${happy}/22; edge/error cases ${edges}/28`);
console.log(`PASS inclusive public boundaries ${acceptedBoundaries.length}/18`);

await testFakeNative(entryUrl);
