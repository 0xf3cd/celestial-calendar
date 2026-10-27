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
import { copyFile, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { assertChartReplay, chartExampleInput, runChartAcceptance } from "./chart_replay.mjs";

export async function testChartApi(celestial, entryUrl, { native = true } = {}) {
  const example = JSON.parse(await readFile(new URL("../../../../docs/examples/chart_snapshot/data.json", import.meta.url), "utf8"));
  const { LAYOUTS, C_DECLARATIONS } = await import(new URL("./bindings.mjs", entryUrl));
  const manifest = { layouts: LAYOUTS };
  const { civil, options } = chartExampleInput(example);
  if (native) {
    assert(Object.isFrozen(celestial.chart));
    console.log(`PASS public chart acceptance ${JSON.stringify(runChartAcceptance(celestial, example))}`);
  }
  const directory = await mkdtemp(resolve(tmpdir(), "celestial-chart-contract-"));
  const source = await readFile(new URL("./index.mjs", entryUrl), "utf8");
  try {
    for (const name of ["bindings.mjs", "validation.mjs"]) {
      await copyFile(new URL(`./${name}`, entryUrl), resolve(directory, name));
    }
    const entry = resolve(directory, "mock-index.mjs");
    await writeFile(entry, source.replace("./celestial-jieqi.mjs", "./mock-module.mjs"));
    await writeFile(resolve(directory, "mock-module.mjs"), `
const layouts = ${JSON.stringify(manifest.layouts)};
const codes = ${JSON.stringify(C_DECLARATIONS.constants)};
const example = ${JSON.stringify(example)};
const field = (layout, name) => layouts[layout].fields.find((item) => item.name === name);
export default async () => {
  const control = globalThis.__chartControl;
  let buffer = new ArrayBuffer(16384);
  const M = {};
  const views = () => {
    for (const [name, Type] of Object.entries({ HEAPU8: Uint8Array, HEAPU16: Uint16Array, HEAP32: Int32Array, HEAPU32: Uint32Array, HEAPF64: Float64Array })) M[name] = new Type(buffer);
  };
  views();
  M._malloc = () => { ++control.allocations; return control.allocationFails ? 0 : 8; };
  M._free = (ptr) => { assertPtr(ptr); ++control.frees; };
  const assertPtr = (ptr) => { if (ptr !== 8) throw new Error("wrong chart pointer"); };
  M.ccall = () => { ++control.errorReads; return control.error; };
  M._celestial_last_error_kind = () => control.kind;
  M._celestial_chart_snapshot = (ptr, ...args) => {
    assertPtr(ptr); control.args = args; ++control.calls;
    if (control.grow) { buffer = buffer.transfer(buffer.byteLength * 2); views(); }
    const set = (base, layout, name, value) => {
      const item = field(layout, name), address = base + item.offset;
      if (item.type === "double") M.HEAPF64[address >> 3] = value;
      else if (item.type === "bool") M.HEAPU8[address] = value;
      else M.HEAPU32[address >> 2] = value;
    };
    const write = (base, layout, record) => {
      for (const [name, value] of Object.entries(record)) set(base, layout, name, value);
    };
    write(ptr, "CelestialChartSnapshot", { valid: 1, ...example.times,
      delta_t_identity_code: args.at(-1) || codes.CELESTIAL_DELTA_T_MODEL_ALGO5,
      nutation_identity_code: codes.CELESTIAL_NUTATION_MODEL_IAU_1980 });
    const bodies = ptr + field("CelestialChartSnapshot", "bodies").offset;
    const absent = new Set();
    example.bodies.forEach((body, index) => {
      const base = bodies + index * layouts.CelestialChartBody.size;
      write(base, "CelestialChartBody", { target_code: index, present_fields: index < 10 ? 3 : 1,
        source_code: codes.CELESTIAL_POSITION_SOURCE_SUN_VSOP87D,
        longitude_deg: body.longitude_deg, latitude_deg: body.latitude_deg,
        distance_au: body.distance_au ?? NaN, longitude_rate_deg_per_tt_day: body.longitude_rate_deg_per_tt_day });
      if (index >= 10) absent.add(String((base + field("CelestialChartBody", "distance_au").offset) >> 3));
    });
    const houses = ptr + field("CelestialChartSnapshot", "houses").offset;
    const { cusps_deg, ...axes } = example.houses;
    write(houses, "CelestialChartHouses", axes);
    const cusps = houses + field("CelestialChartHouses", "cusps_deg").offset;
    cusps_deg.forEach((value, index) => { M.HEAPF64[(cusps >> 3) + index] = value; });
    if (control.mutate) control.mutate(M, { ptr, bodies, houses, cusps, layouts });
    if (control.forbidAbsent) M.HEAPF64 = new Proxy(M.HEAPF64, {
      get(target, key) { if (absent.has(key)) throw new Error("read absent node storage"); return Reflect.get(target, key, target); },
    });
  };
  control.M = M;
  return M;
};
`);
    const control = globalThis.__chartControl = { allocations: 0, frees: 0, calls: 0, errorReads: 0, kind: 0, error: "stale error" };
    const fixture = await import(pathToFileURL(entry));
    const forbidden = new Proxy({}, { get() { throw new Error("read before init"); } });
    assert.throws(() => fixture.chart.snapshot(forbidden, forbidden), { name: "CelestialError", operation: "chart.snapshot", recorded: false });
    await fixture.init();
    const call = (input = civil, config = options) => fixture.chart.snapshot(input, config);
    const offset = (layout, name) => manifest.layouts[layout].fields.find((item) => item.name === name).offset;
    const scenarios = {
      dispatch() {
        assertChartReplay(call(), example);
        call({ year: 2026, month: 3, day: 17, fraction: 0.123 }, {
          scale: "ut1", location: { latitudeDeg: -33.87, longitudeDeg: -151.21 }, system: "wholeSign", model: "algo2",
        });
        assert.deepEqual(control.args, [2026, 3, 17, 0.123, 1, -33.87, -151.21, 1, 2]);
        for (const model of [undefined, "default", "algo1", "algo2", "algo3", "algo4", "algo5"]) {
          call(civil, { ...options, model });
          assert.equal(control.args.at(-1), model === undefined || model === "default" ? 0 : Number(model.at(-1)));
        }
        for (const [index, system] of ["equal", "wholeSign", "placidus"].entries()) {
          call(civil, { ...options, system }); assert.equal(control.args.at(-2), index);
        }
        for (const [index, scale] of ["utc", "ut1"].entries()) {
          call(civil, { ...options, scale }); assert.equal(control.args[4], index);
        }
      },
      modelIdentity() {
        let inheritedReads = 0;
        for (const model of [undefined, "algo1"]) {
          const inheritedValue = Object.assign(Object.create({ model }), { scale: "utc", location: options.location, system: "equal" });
          assert.throws(() => call(civil, inheritedValue), TypeError);
          const inherited = Object.assign(Object.create({ get model() { ++inheritedReads; return model; } }), { scale: "utc", location: options.location, system: "equal" });
          assert.throws(() => call(civil, inherited), TypeError);
        }
        assert.equal(inheritedReads, 0);

        let modelDescriptors = 0;
        call(civil, new Proxy({ ...options, model: "algo1" }, {
          getOwnPropertyDescriptor(target, key) {
            if (key === "model" && ++modelDescriptors > 1) return undefined;
            return Reflect.getOwnPropertyDescriptor(target, key);
          },
        }));
        assert.equal(modelDescriptors, 1, "model ownership queried twice");
        assert.equal(control.args.at(-1), 1, "model silently defaulted through a changing descriptor");
      },
      inputValidation() {
        const reads = {};
        const once = (record, prefix) => new Proxy(record, { get(target, key) {
          reads[`${prefix}.${String(key)}`] = (reads[`${prefix}.${String(key)}`] ?? 0) + 1;
          assert.equal(reads[`${prefix}.${String(key)}`], 1, "field read twice"); return target[key];
        } });
        call(once({ ...civil }, "civil"), once({ ...options, location: once({ ...options.location }, "location") }, "options"));
        assert.equal(Object.keys(reads).length, 10);

        const badInputs = [null, [], new Date(), { ...civil, isLeap: false }, { ...civil, year: 2 ** 31 }, { ...civil, fraction: NaN }];
        for (const input of badInputs) assert.throws(() => call(input), (error) => error instanceof TypeError || error instanceof RangeError);
        for (const config of [null, [], {}, { ...options, model: null }, { ...options, scale: "UTC" }, { ...options, system: "unknown" },
          { ...options, location: { latitudeDeg: 0, longitudeDeg: Infinity } }, Object.create(options)]) {
          assert.throws(() => call(civil, config), (error) => error instanceof TypeError || error instanceof RangeError);
        }
        for (const input of [Object.create(civil), Object.assign(Object.create({ year: civil.year }), {
          month: civil.month, day: civil.day, fraction: civil.fraction,
        })]) assert.throws(() => call(input), TypeError);
        assert.throws(() => call(civil, { ...options, location: Object.create(options.location) }), TypeError);

        let unknownReads = 0;
        for (const key of ["modle", "deltaTModel", Symbol("model")]) {
          for (const enumerable of [true, false]) {
            const extra = Object.defineProperty({ ...options }, key, {
              enumerable, get() { ++unknownReads; throw new Error("unknown getter was read"); },
            });
            const allocations = control.allocations;
            assert.throws(() => call(civil, extra), TypeError);
            assert.equal(control.allocations, allocations, "unknown key reached native allocation");
          }
        }
        assert.equal(unknownReads, 0);
        call({ ...civil, extra: "permitted" }, {
          ...options, location: { ...options.location, extra: "permitted" },
        });
      },
      storageIndependence() {
        const oldViews = control.M.HEAPF64;
        const retained = call(); const retainedJson = JSON.stringify(retained);
        control.grow = true; control.forbidAbsent = true;
        assertChartReplay(call(), example);
        assert.equal(oldViews.byteLength, 0, "growth did not detach old views");
        assert.equal(JSON.stringify(retained), retainedJson);
        control.grow = false; control.forbidAbsent = false;
      },
      malformedNative() {
        const mutations = [
          (M, p) => { M.HEAPU8[p.ptr] = 2; },
          (M, p) => { M.HEAPF64[(p.ptr + offset("CelestialChartSnapshot", "jd_ut1")) >> 3] = NaN; },
          (M, p) => { M.HEAPU32[p.bodies >> 2] = 1; },
          (M, p) => { M.HEAPU32[(p.bodies + offset("CelestialChartBody", "present_fields")) >> 2] = 1; },
          (M, p) => { M.HEAPU32[(p.bodies + 10 * manifest.layouts.CelestialChartBody.size + offset("CelestialChartBody", "present_fields")) >> 2] = 3; },
          (M, p) => { M.HEAPU32[(p.bodies + offset("CelestialChartBody", "present_fields")) >> 2] = 0x10000003; },
          (M, p) => { M.HEAPU32[(p.bodies + offset("CelestialChartBody", "source_code")) >> 2] = 0x10000001; },
          (M, p) => { M.HEAPU32[(p.ptr + offset("CelestialChartSnapshot", "delta_t_identity_code")) >> 2] = 0; },
          (M, p) => { M.HEAPU32[(p.ptr + offset("CelestialChartSnapshot", "nutation_identity_code")) >> 2] = 0x10000002; },
          (M, p) => { M.HEAPF64[(p.bodies + offset("CelestialChartBody", "longitude_deg")) >> 3] = 360; },
          (M, p) => { M.HEAPF64[(p.bodies + offset("CelestialChartBody", "latitude_deg")) >> 3] = 91; },
          (M, p) => { M.HEAPF64[(p.bodies + 10 * manifest.layouts.CelestialChartBody.size + offset("CelestialChartBody", "latitude_deg")) >> 3] = 0.5; },
          (M, p) => { M.HEAPF64[(p.bodies + offset("CelestialChartBody", "distance_au")) >> 3] = 0; },
          (M, p) => { M.HEAPF64[(p.bodies + offset("CelestialChartBody", "longitude_rate_deg_per_tt_day")) >> 3] = Infinity; },
          (M, p) => { M.HEAPF64[p.houses >> 3] = -1; },
          (M, p) => { M.HEAPF64[p.cusps >> 3] = NaN; },
        ];
        for (const mutate of mutations) {
          control.mutate = mutate;
          const errorReads = control.errorReads;
          assert.throws(call, { name: "CelestialError", operation: "chart.snapshot", recorded: false });
          assert.equal(control.errorReads, errorReads, "malformed success read stale last_error");
        }
        control.error = "native chart failure";
        control.kind = C_DECLARATIONS.constants.CELESTIAL_ERROR_COMPUTATION;
        control.mutate = (M, p) => { M.HEAPU8[p.ptr] = 0; };
        assert.throws(call, { name: "CelestialError", operation: "chart.snapshot", recorded: true, message: "native chart failure" });
        control.mutate = undefined;
        control.kind = 0;
        assertChartReplay(call(), example);
      },
      allocationCleanup() {
        assert.equal(control.allocations, control.frees, "chart output allocations leaked");
        control.allocationFails = true;
        const calls = control.calls;
        assert.throws(call, { name: "CelestialError", operation: "chart.snapshot", recorded: false });
        assert.equal(control.calls, calls, "native call after allocation failure");
      },
    };
    for (const [name, scenario] of Object.entries(scenarios)) {
      scenario();
      console.log(`PASS chart ${name}`);
    }

    if (!native) return 1;

    await copyFile(new URL("./celestial-jieqi.wasm", entryUrl), resolve(directory, "celestial-jieqi.wasm"));
    const factoryUrl = new URL("./celestial-jieqi.mjs", entryUrl).href;
    await writeFile(resolve(directory, "instrumented.mjs"), `
import factory from ${JSON.stringify(factoryUrl)};
export default async (options) => {
  const M = await factory(options), raw = M._celestial_chart_snapshot;
  M._celestial_chart_snapshot = (ptr, ...args) => { raw(ptr, ...args); if (globalThis.__chartNativeMutation) globalThis.__chartNativeMutation(M, ptr); };
  return M;
};
`);
    const realEntry = resolve(directory, "real-index.mjs");
    await writeFile(realEntry, source.replace("./celestial-jieqi.mjs", "./instrumented.mjs"));
    const real = await import(pathToFileURL(realEntry)); await real.init();
    const replay = () => assertChartReplay(real.chart.snapshot(civil, options), example);
    replay();
    const bodiesOffset = offset("CelestialChartSnapshot", "bodies");
    for (const nativeOffset of [
      bodiesOffset + 2 * manifest.layouts.CelestialChartBody.size + offset("CelestialChartBody", "longitude_deg"),
      bodiesOffset + 2 * manifest.layouts.CelestialChartBody.size + offset("CelestialChartBody", "longitude_rate_deg_per_tt_day"),
      offset("CelestialChartSnapshot", "houses") + offset("CelestialChartHouses", "cusps_deg"),
    ]) {
      globalThis.__chartNativeMutation = (M, ptr) => { M.HEAPF64[(ptr + nativeOffset) >> 3] += 1; };
      assert.throws(replay, /replay residual/);
    }
    delete globalThis.__chartNativeMutation;
    replay();
    console.log("PASS real native-return Mercury/rate/cusp defect controls 3/3; restored replay");
  } finally {
    delete globalThis.__chartControl;
    delete globalThis.__chartNativeMutation;
    await rm(directory, { recursive: true, force: true });
  }
  return 1;
}
