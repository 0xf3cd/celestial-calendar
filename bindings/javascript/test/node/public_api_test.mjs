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
import { copyFile, mkdtemp, readFile, rename, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { API, SymbolFlags } from "typescript/unstable/sync";
import { isLiteralTypeNode, isNumericLiteral, isTypeLiteralNode, isTypeReferenceNode } from "typescript/unstable/ast";

if (process.argv.length !== 3) throw new Error("usage: node public_api_test.mjs <staged index.mjs>");

const entryUrl = pathToFileURL(resolve(process.argv[2]));
const wasmPath = fileURLToPath(new URL("./celestial-jieqi.wasm", entryUrl));
const heldWasmPath = `${wasmPath}.init-failure-probe`;
const celestial = await import(entryUrl);
const declarationPath = fileURLToPath(new URL("./index.d.ts", entryUrl));
const declarationText = await readFile(declarationPath, "utf8");
const namespaces = ["config", "time", "sun", "moon", "jieqi", "lunar"];

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
    return { values: values.map((symbol) => symbol.name).sort(), members, jieqi };
  } finally {
    api.close();
  }
};
const assertSurface = (runtime, declared) => {
  assert.deepEqual(Object.keys(runtime).sort(), declared.values, "runtime/staged declaration value exports");
  for (const name of namespaces) {
    assert.deepEqual(Object.keys(runtime[name]).sort(), declared.members[name], `${name} runtime/declaration members`);
  }
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
console.log("PASS staged runtime/declaration surface; drift mutations 4/4");

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
  ["CelestialError", "Jieqi", "config", "init", "jieqi", "lunar", "moon", "sun", "time"],
  "public module exports",
);
assert.deepEqual(Object.keys(celestial.config), ["setLogVerbosity"]);
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

let edges = 0;
const edge = (label, action, ErrorType) => {
  assert.throws(action, ErrorType, label);
  ++edges;
};

assert.throws(
  () => celestial.moon.illumination(2448724.5),
  { name: "Error", message: "Call and await init() before moon.illumination()." },
  "pre-init call",
);
++edges;

await rename(wasmPath, heldWasmPath);
let failedInitialization;
const initializationErrors = [];
const originalConsoleError = console.error;
try {
  console.error = (...args) => initializationErrors.push(args.join(" "));
  failedInitialization = celestial.init();
  assert.strictEqual(celestial.init(), failedInitialization, "concurrent init shares one promise");
  await assert.rejects(failedInitialization);
} finally {
  console.error = originalConsoleError;
  await rename(heldWasmPath, wasmPath);
}
assert(initializationErrors.length > 0, "failed init reported its load error");

const retry = celestial.init();
assert.notStrictEqual(retry, failedInitialization, "failed init can retry");
await retry;
assert.strictEqual(celestial.init(), retry, "completed init reuses one promise");

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

check("config.setLogVerbosity", () => assert.equal(celestial.config.setLogVerbosity("none"), undefined));
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
  finite(value.longitudeDeg, value.latitudeDeg, value.radiusAu);
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

edge("log level type", () => celestial.config.setLogVerbosity(true), TypeError);
edge("log level value", () => celestial.config.setLogVerbosity("trace"), RangeError);
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
assert(recordingError instanceof celestial.CelestialError);
assert.equal(recordingError.operation, "time.localApparentSiderealTime");
assert.equal(recordingError.recorded, true);
++edges;

let lunarError;
try {
  celestial.lunar.fromGregorian("algo1", { year: 1900, month: 1, day: 1 });
} catch (error) {
  lunarError = error;
}
assert(lunarError instanceof celestial.CelestialError);
assert.equal(lunarError.operation, "lunar.fromGregorian");
assert.equal(lunarError.recorded, true);
assert.match(lunarError.message, /cannot be represented/);
++edges;

assert.equal(celestial.jieqi.name(0), "立春", "module survives translated errors");
assert.equal(happy, 22, "public method denominator");
assert.equal(edges, 30, "public edge denominator");
assert.equal(acceptedBoundaries.length, 16, "public acceptance denominator");
console.log(`PASS public methods ${happy}/22; edge/error cases ${edges}/30`);
console.log(`PASS inclusive public boundaries ${acceptedBoundaries.length}/16`);

const fixtureDirectory = await mkdtemp(resolve(tmpdir(), "celestial-js-contract-"));
try {
  const fixtureEntry = resolve(fixtureDirectory, "index.mjs");
  const source = await readFile(fileURLToPath(entryUrl), "utf8");
  await writeFile(fixtureEntry, source.replace("./celestial-jieqi.mjs", "./mock-module.mjs"), "utf8");
  await copyFile(fileURLToPath(new URL("./bindings.mjs", entryUrl)), resolve(fixtureDirectory, "bindings.mjs"));
  await copyFile(fileURLToPath(new URL("./validation.mjs", entryUrl)), resolve(fixtureDirectory, "validation.mjs"));
  await writeFile(
    resolve(fixtureDirectory, "mock-module.mjs"),
    `export default async () => {
  const buffer = new ArrayBuffer(65_536);
  const M = {
    HEAPU8: new Uint8Array(buffer),
    HEAPU16: new Uint16Array(buffer),
    HEAP32: new Int32Array(buffer),
    HEAPU32: new Uint32Array(buffer),
    HEAPF64: new Float64Array(buffer),
  };
  let next = 8;
  let lastError = "";
  M._malloc = (bytes) => {
    if (globalThis.__celestialFailAllocation === bytes) return 0;
    const ptr = next;
    next += Math.ceil(bytes / 8) * 8;
    return ptr;
  };
  M._free = () => {};
  M._new_moons_after_jde = (_jde, _slots, count) => count;
  M._moon_phase_moments = (_year, _phase, countPtr, slots) => {
    if (slots === 0) {
      M.HEAPU32[countPtr >> 2] = 2;
      return 0;
    }
    lastError = "native phase fill failed";
    M.HEAPU32[countPtr >> 2] = 0;
    return 0;
  };
  M._sun_apparent_geocentric_coord = M._moon_apparent_geocentric_coord = (ptr) => {
    M.HEAPU8[ptr] = 0;
    lastError = "native coordinate failed";
  };
  M._get_supported_lunar_year_range = (ptr, algorithm) => {
    globalThis.__celestialLunarCalls.push(["range", algorithm]);
    M.HEAPU8[ptr] = globalThis.__celestialFailRange ? 0 : 1;
    M.HEAP32[(ptr + 4) >> 2] = globalThis.__celestialLunarRange.start;
    M.HEAP32[(ptr + 8) >> 2] = globalThis.__celestialLunarRange.end;
    lastError = globalThis.__celestialFailRange ? "native lunar range query failed" : "";
  };
  M._get_lunar_year_info = (ptr, algorithm, year) => {
    globalThis.__celestialLunarCalls.push(["yearInfo", algorithm, year]);
    M.HEAPU8[ptr] = 1;
    M.HEAP32[(ptr + 4) >> 2] = year;
    M.HEAPU8[ptr + 8] = 1;
    M.HEAPU8[ptr + 9] = 1;
  };
  M._lunar_to_gregorian = (ptr, algorithm, year, month, isLeap, day) => {
    globalThis.__celestialLunarCalls.push(["toGregorian", algorithm, year, month, isLeap, day]);
    M.HEAPU8[ptr] = 1;
    M.HEAP32[(ptr + 4) >> 2] = year;
    M.HEAPU8[ptr + 8] = month;
    M.HEAPU8[ptr + 9] = day;
  };
  M.ccall = () => lastError;
  return M;
};
`,
    "utf8",
  );

  globalThis.__celestialLunarCalls = [];
  globalThis.__celestialLunarRange = { start: 2024, end: 2024 };
  const fixture = await import(pathToFileURL(fixtureEntry));
  assert.deepEqual(globalThis.__celestialLunarCalls, [], "no import-time lunar range query");
  await fixture.init();
  assert.deepEqual(globalThis.__celestialLunarCalls, [], "no init-time lunar range query");
  assert.equal(fixture.moon.newMoonsAfter(2451545.0, 4096).length, 4096, "count 4096 accepted");

  globalThis.__celestialFailAllocation = 4096 * 8;
  assert.throws(
    () => fixture.moon.newMoonsAfter(2451545.0, 4096),
    {
      name: "CelestialError",
      message: "moon.newMoonsAfter failed to allocate the WASM output buffer.",
      operation: "moon.newMoonsAfter",
      recorded: false,
    },
    "allocation failure",
  );
  delete globalThis.__celestialFailAllocation;

  assert.throws(
    () => fixture.moon.phaseMoments(2024, "new"),
    {
      name: "CelestialError",
      message: "native phase fill failed",
      operation: "moon.phaseMoments",
      recorded: true,
    },
    "recording fill failure",
  );
  console.log("PASS count boundary 4096/4097; allocation failure; recording fill reason");

  for (const namespace of ["sun", "moon"]) {
    const operation = `${namespace}.apparentGeocentricCoordinate`;
    assert.throws(
      () => fixture[namespace].apparentGeocentricCoordinate(2451545.0),
      { name: "CelestialError", operation, message: "native coordinate failed", recorded: true },
      "singular coordinate error operation",
    );
  }

  for (const algorithm of ["algo1", "algo2", "algo3"]) {
    const nativeAlgorithm = Number(algorithm.slice(-1));
    for (const method of ["yearInfo", "toGregorian"]) {
      const call = (year) => method === "yearInfo"
        ? fixture.lunar.yearInfo(algorithm, year)
        : fixture.lunar.toGregorian(algorithm, { year, month: 1, day: 1, isLeap: false });
      for (const year of [2024, 2025]) {
        globalThis.__celestialLunarRange = { start: year, end: year };
        globalThis.__celestialLunarCalls = [];
        call(year);
        assert.deepEqual(
          globalThis.__celestialLunarCalls,
          [
            ["range", nativeAlgorithm],
            method === "yearInfo" ? [method, nativeAlgorithm, year] : [method, nativeAlgorithm, year, 1, false, 1],
          ],
          `${algorithm} ${method}: one fresh native range query before use`,
        );
      }
      for (const year of [2024, 2026]) {
        globalThis.__celestialLunarCalls = [];
        assert.throws(() => call(year), RangeError, `${method}: actual native range rejects year ${year}`);
        assert.deepEqual(globalThis.__celestialLunarCalls, [["range", nativeAlgorithm]], "no native conversion after invalid year");
      }
      globalThis.__celestialLunarCalls = [];
      assert.throws(() => call("2025"), TypeError, `${method}: invalid year type`);
      assert.deepEqual(globalThis.__celestialLunarCalls, [["range", nativeAlgorithm]], "range query precedes year validation");
      globalThis.__celestialFailRange = true;
      try {
        for (const year of [2025, "invalid"]) {
          globalThis.__celestialLunarCalls = [];
          assert.throws(
            () => call(year),
            {
              name: "CelestialError",
              operation: `lunar.${method}`,
              message: "native lunar range query failed",
              recorded: true,
            },
            `${method}: range-query error retains caller and native details before year validation`,
          );
          assert.deepEqual(globalThis.__celestialLunarCalls, [["range", nativeAlgorithm]]);
        }
      } finally {
        delete globalThis.__celestialFailRange;
      }
    }
  }
  console.log("PASS singular error operations; per-call native lunar ranges, validation order and recorded failures");
} finally {
  delete globalThis.__celestialFailAllocation;
  delete globalThis.__celestialLunarCalls;
  delete globalThis.__celestialLunarRange;
  delete globalThis.__celestialFailRange;
  await rm(fixtureDirectory, { recursive: true, force: true });
}
