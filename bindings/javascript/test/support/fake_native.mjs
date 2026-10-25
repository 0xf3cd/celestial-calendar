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

export async function testFakeNative(entryUrl) {
  const { BINDINGS, C_DECLARATIONS, LAYOUTS, PROTOCOLS } = await import(new URL("./bindings.mjs", entryUrl));
  assert(C_DECLARATIONS?.constants, "generated C_DECLARATIONS is required before integration acceptance");
  const codes = C_DECLARATIONS.constants;
  const bindings = Object.fromEntries(BINDINGS.map((binding) => [binding.cName, binding]));
  const directory = await mkdtemp(resolve(tmpdir(), "celestial-native-contract-"));
  const control = {};
  const M = {};
  const reset = (changes = {}) => {
    assert.equal(control.live?.size ?? 0, 0, "previous call leaked WASM allocations");
    Object.assign(control, {
      next: 8, allocations: 0, frees: 0, live: new Map(), calls: [], failAllocation: 0,
      kind: 0, message: "stale message", errorReads: 0, count: 2, total: 2,
      countWritten: 0, fillWritten: 2, failPass: undefined, mutate: undefined,
      string: new TextEncoder().encode("立春\0"), stringMutation: undefined,
      grow: false, ...changes,
    });
  };
  const views = (buffer) => {
    for (const [name, Type] of Object.entries({
      HEAPU8: Uint8Array, HEAPU16: Uint16Array, HEAP32: Int32Array,
      HEAPU32: Uint32Array, HEAPF64: Float64Array,
    })) M[name] = new Type(buffer);
  };
  views(new ArrayBuffer(1_048_576));
  M._malloc = (bytes) => {
    if (++control.allocations === control.failAllocation) return 0;
    const ptr = control.next;
    control.next += Math.ceil(bytes / 8) * 8;
    assert(control.next <= M.HEAPU8.length, "fake heap exhausted");
    M.HEAPU8.fill(0, ptr, control.next);
    control.live.set(ptr, bytes);
    return ptr;
  };
  M._free = (ptr) => {
    assert(control.live.delete(ptr), "free must match a successful allocation exactly once");
    ++control.frees;
  };
  M._celestial_last_error_kind = () => control.kind;
  M.ccall = (name, result, types, args) => {
    assert.equal(name, "celestial_last_error");
    assert.equal(result, "string");
    assert.deepEqual([types, args], [[], []]);
    ++control.errorReads;
    return control.message;
  };
  const called = (name, args, pass = "sret") => {
    control.calls.push([name, ...args]);
    if (control.grow) {
      views(M.HEAPU8.buffer.transfer(M.HEAPU8.byteLength * 2));
      control.grow = false;
    }
    if (control.failPass === pass) {
      control.kind = control.errorKind;
      return false;
    }
    return true;
  };
  const write = (name, ptr, record) => {
    const layout = LAYOUTS[bindings[name].result.slice("sret:".length)];
    for (const field of layout.fields) {
      const address = ptr + field.offset;
      const value = record[field.name] ?? 0;
      switch (field.type) {
        case "bool": case "uint8_t": M.HEAPU8[address] = value; break;
        case "uint16_t": M.HEAPU16[address >> 1] = value; break;
        case "int32_t": M.HEAP32[address >> 2] = value; break;
        case "uint32_t": M.HEAPU32[address >> 2] = value; break;
        case "double": M.HEAPF64[address >> 3] = value; break;
        default: assert.fail(`unexpected scalar fake field ${field.type}`);
      }
    }
    control.mutate?.(M, ptr, layout);
  };
  for (const binding of BINDINGS.filter(({ result }) => result.startsWith("sret:"))) {
    if (binding.cName === "celestial_chart_snapshot") continue;
    M[binding.wasmName] = (ptr, ...args) => {
      const success = called(binding.cName, args);
      write(binding.cName, ptr, {
        valid: success ? 1 : 0, value: 12.5, lon: 120, lat: 1, r: 2,
        year: 2024, month: 1, day: 1, fraction: 0.5,
        y: 2024, m: 1, d: 1, frac: 0.5, jq_idx: args[1] ?? 0,
        count: control.count, start: 1600, end: 2199, leap_month: 0, month_len: 0,
        model_identity_code: args[1] || codes.CELESTIAL_DELTA_T_MODEL_ALGO5,
      });
    };
  }
  M._celestial_moon_phase_moments = (year, phase, countPtr, slots, capacity) => {
    const pass = slots === 0 ? "count" : "fill";
    const success = called("celestial_moon_phase_moments", [year, phase], pass);
    M.HEAPU32[countPtr >> 2] = success ? (slots === 0 ? control.count : control.total) : 0;
    if (slots === 0 || !success) return success ? control.countWritten : 0;
    for (let index = 0; index < capacity; ++index) M.HEAPF64[(slots >> 3) + index] = 2451545 + index;
    control.mutate?.(M, slots);
    return control.fillWritten;
  };
  M._celestial_new_moons_in_year = (year, ...args) => M._celestial_moon_phase_moments(year, 0, ...args);
  const requested = (name) => (...args) => {
    const [slots, capacity] = args.slice(-2);
    if (!called(name, args, "fill")) return 0;
    for (let index = 0; index < capacity; ++index) M.HEAPF64[(slots >> 3) + index] = 2451545 + index;
    control.mutate?.(M, slots);
    return control.fillWritten === 2 ? capacity : control.fillWritten;
  };
  M._celestial_solar_lon_roots = requested("celestial_solar_lon_roots");
  M._celestial_new_moons_after_jde = requested("celestial_new_moons_after_jde");
  M._celestial_get_jieqi_name = (index, ptr, capacity) => {
    if (!called("celestial_get_jieqi_name", [index, capacity], ptr === 0 ? "count" : "fill")) return 0;
    if (ptr !== 0) {
      assert.equal(capacity, control.live.get(ptr), "string capacity must match its allocation");
      M.HEAPU8.set(control.string, ptr);
    }
    return control.stringMutation?.(M, ptr, capacity) ?? control.string.length;
  };

  try {
    reset();
    globalThis.__celestialFakeFactory = async () => {
      await globalThis.__celestialFakeLoad;
      return M;
    };
    const entry = resolve(directory, "index.mjs");
    const source = await readFile(new URL("./index.mjs", entryUrl), "utf8");
    await writeFile(entry, source.replace("./celestial-jieqi.mjs", "./mock-module.mjs"));
    for (const name of ["bindings.mjs", "validation.mjs"]) {
      await copyFile(new URL(`./${name}`, entryUrl), resolve(directory, name));
    }
    await writeFile(resolve(directory, "mock-module.mjs"), "export default () => globalThis.__celestialFakeFactory();\n");
    const fixture = await import(pathToFileURL(entry));
    assert.deepEqual(control.calls, [], "import does not query native domains");
    let previous;
    for (const rejection of [new Error("original loader failure"), { loaderFailure: true }]) {
      let rejectLoad;
      globalThis.__celestialFakeLoad = new Promise((_resolve, reject) => { rejectLoad = reject; });
      const pending = fixture.init();
      assert.notStrictEqual(pending, previous);
      assert.strictEqual(fixture.init(), pending);
      assert.throws(() => fixture.moon.newMoonsAfter(NaN, 0), { name: "CelestialError", recorded: false });
      rejectLoad(rejection);
      await assert.rejects(pending, (error) => error === rejection);
      previous = pending;
    }
    delete globalThis.__celestialFakeLoad;
    const initialized = fixture.init();
    assert.equal(await initialized, undefined);
    assert.strictEqual(fixture.init(), initialized);
    assert.deepEqual(control.calls, [], "initialization does not query native domains");
    assert.equal("config" in fixture, false);
    assert.deepEqual(Object.keys(fixture).sort(), ["CelestialError", "Jieqi", "chart", "init", "jieqi", "lunar", "moon", "sun", "time"]);
    assert.deepEqual(fixture.sun.apparentGeocentricCoordinate(2451545), { longitudeDeg: 120, latitudeDeg: 1, distanceAu: 2 });
    assert.deepEqual(fixture.moon.apparentGeocentricCoordinate(2451545), { longitudeDeg: 120, latitudeDeg: 1, distanceKm: 2 });

    const phase = () => fixture.moon.phaseMoments(2024, "new");
    const name = () => fixture.jieqi.name(0);
    const scalar = () => fixture.time.deltaT(2024);
    const libraryError = (action, recorded = false) => {
      assert.throws(action, (error) => error instanceof fixture.CelestialError && error.recorded === recorded);
      assert.equal(control.live.size, 0, "failure leaked WASM memory");
    };
    for (const [model, code] of [[undefined, 0], ["default", 0], ["algo5", 5]]) {
      reset();
      assert.equal(fixture.time.deltaT(2024, model), 12.5);
      assert.deepEqual(control.calls, [["celestial_delta_t", 2024, code]]);
      assert.equal(control.errorReads, 0, "successful call must not read stale error text");
    }
    for (const [kind, ErrorType] of [[1, RangeError], [2, fixture.CelestialError], [3, fixture.CelestialError], [4, fixture.CelestialError]]) {
      for (const message of ["native failure", ""]) {
        reset({ failPass: "sret", errorKind: kind, message });
        assert.throws(scalar, (error) => error instanceof ErrorType && error.message.length > 0
          && (kind === 1 || error.recorded === true));
        assert.equal(control.live.size, 0);
        assert.equal(control.kind, kind, "reader changed the error kind");
        assert.equal(control.message, message, "reader changed the error message");
      }
    }
    reset({ failPass: "sret", errorKind: 0x10000001 }); libraryError(scalar);
    reset({ mutate: () => { control.kind = 2; control.message = "error despite valid marker"; } });
    libraryError(scalar, true);
    for (const pass of ["count", "fill"]) {
      reset({ failPass: pass, errorKind: 2, message: "native phase failure" });
      libraryError(phase, true);
      reset({ failPass: pass, errorKind: 1, message: "invalid native phase" });
      assert.throws(phase, RangeError);
      assert.equal(control.live.size, 0);
    }
    for (const changes of [{ countWritten: 1 }, { total: 1 }, { total: 3 }, { fillWritten: 1 }, { fillWritten: 3 }]) {
      reset(changes); libraryError(phase);
    }
    reset({ count: 0 });
    assert.equal(PROTOCOLS.celestial_moon_phase_moments.legal_empty, false, "annual phases require a nonempty native result");
    assert.equal(PROTOCOLS.celestial_new_moons_in_year.legal_empty, false, "annual new moons require a nonempty native result");
    libraryError(phase);
    reset();
    assert.deepEqual(fixture.moon.newMoonsInYear(2024), [2451545, 2451546]);
    reset({ count: 0 }); libraryError(() => fixture.moon.newMoonsInYear(2024));
    const protocol = PROTOCOLS.celestial_moon_phase_moments;
    const protocolVariants = [
      ["empty", { ...protocol, legal_empty: true }],
      ["missing", undefined], ["null", null], ["array", []],
      ["empty-string", { ...protocol, legal_empty: "false" }],
      ["empty-number", { ...protocol, legal_empty: 0 }],
      ["empty-missing", { ...protocol, legal_empty: undefined }],
      ["total-false", { ...protocol, total_must_match: false }],
      ["total-string", { ...protocol, total_must_match: "true" }],
      ["total-missing", { ...protocol, total_must_match: undefined }],
      ["written-policy", { ...protocol, written_policy: "exact-requested-count" }],
      ["written-missing", { ...protocol, written_policy: undefined }],
      ["error-reader", { ...protocol, error_channel: "preserve" }],
      ["error-missing", { ...protocol, error_channel: undefined }],
      ["kind", { ...protocol, kind: "sret" }],
      ["null-slots", { ...protocol, null_slots_requires_zero_capacity: false }],
      ["count-type", { ...protocol, count_parameter: 1 }],
      ["count-role", { ...protocol, count_parameter: "slots" }],
      ["slots-empty", { ...protocol, slots_parameter: "" }],
      ["capacity-missing", { ...protocol, capacity_parameter: undefined }],
      ["missing-map", undefined],
    ];
    for (const [variant, candidate] of protocolVariants) {
      await writeFile(resolve(directory, `${variant}-bindings.mjs`), `
export * from "./bindings.mjs";
import { PROTOCOLS as protocols } from "./bindings.mjs";
export const PROTOCOLS = ${variant === "missing-map" ? "undefined"
    : `{ ...protocols, celestial_moon_phase_moments: ${JSON.stringify(candidate) ?? "undefined"} }`};
`);
      await writeFile(resolve(directory, `${variant}-index.mjs`), source
        .replace("./celestial-jieqi.mjs", "./mock-module.mjs")
        .replace("./bindings.mjs", `./${variant}-bindings.mjs`));
      const alternate = await import(pathToFileURL(resolve(directory, `${variant}-index.mjs`)));
      await alternate.init();
      reset({ count: 0 });
      if (variant === "empty") {
        assert.deepEqual(alternate.moon.phaseMoments(2024, "new"), [], "declared legal empty result");
        assert.equal(control.calls.length, 1, "empty success must not fill");
        reset({ count: 0, failPass: "count", errorKind: 3, message: "" });
        assert.throws(() => alternate.moon.phaseMoments(2024, "new"), alternate.CelestialError);
      } else {
        assert.throws(() => alternate.moon.phaseMoments(2024, "new"), (error) =>
          error instanceof alternate.CelestialError && error.operation === "moon.phaseMoments"
          && error.recorded === false && error.message === "Invalid native result: count/fill protocol.");
        assert.equal(control.calls.length, 0, "invalid protocol must fail before the native call");
        assert.equal(control.allocations, 0, "invalid protocol must fail before allocation");
      }
      assert.equal(control.live.size, 0);
    }
    console.log(`PASS generated count/fill policies: legal empty control and ${protocolVariants.length - 1} malformed-metadata controls`);
    reset({ count: 0 });
    assert.deepEqual(fixture.sun.longitudeCrossings(2024, 0), [], "legal empty solar result");
    reset({ count: 0xFFFFFFFF }); libraryError(phase);
    reset({ count: (16 * 1024 * 1024) / 8, failAllocation: 2 });
    libraryError(phase);
    assert.equal(control.allocations, 2, "the exact 16 MiB limit reaches malloc");
    reset({ count: (16 * 1024 * 1024) / 8 + 1 });
    libraryError(phase);
    assert.equal(control.allocations, 1, "an oversized output is rejected before the second allocation");
    reset({ mutate: (heap, slots) => { heap.HEAPF64[slots >> 3] = NaN; } }); libraryError(phase);
    reset({ grow: true }); assert.deepEqual(phase(), [2451545, 2451546]);

    const civil = { year: 2024, month: 1, day: 1, fraction: 0.5 };
    const options = { scale: "ut1", location: { latitudeDeg: 0, longitudeDeg: 0 }, system: "equal" };
    const allocations = [
      [() => fixture.time.ut1ToJd(civil), 1],
      [phase, 1], [phase, 2],
      [() => fixture.sun.longitudeCrossings(2024, 0), 1],
      [() => fixture.sun.longitudeCrossings(2024, 0), 2],
      [() => fixture.moon.newMoonsAfter(2451545, 2), 1],
      [name, 1], [() => fixture.chart.snapshot(civil, options), 1],
    ];
    for (const [action, failAllocation] of allocations) {
      reset({ failAllocation }); libraryError(action);
      assert.equal(control.allocations - 1, control.frees, "partial allocation cleanup");
    }
    for (const [fieldName, value] of [["valid", 0], ["valid", 2], ["value", NaN], ["model_identity_code", 0], ["model_identity_code", 0x10000005]]) {
      reset({ mutate: (heap, ptr, layout) => {
        const field = layout.fields.find(({ name }) => name === fieldName);
        if (field.type === "double") heap.HEAPF64[(ptr + field.offset) >> 3] = value;
        else if (field.type === "bool") heap.HEAPU8[ptr + field.offset] = value;
        else heap.HEAPU32[(ptr + field.offset) >> 2] = value;
      } });
      libraryError(scalar);
      assert.equal(control.errorReads, 0);
    }
    reset({ mutate: (heap, ptr, layout) => {
      const field = layout.fields.find(({ name }) => name === "model_identity_code");
      heap.HEAPU32[(ptr + field.offset) >> 2] = 2;
    } });
    libraryError(() => fixture.time.deltaT(2024, "algo5"));
    for (const string of ["立春\0", `${"x".repeat(40)}\0`]) {
      reset({ string: new TextEncoder().encode(string) });
      assert.equal(name(), string.slice(0, -1), "sized string is not limited to a fixed buffer");
    }
    for (const stringMutation of [
      (_heap, _ptr, capacity) => capacity === 0 ? 0 : capacity,
      (_heap, _ptr, capacity) => capacity === 0 ? 7 : 8,
      (heap, ptr, capacity) => { if (capacity) heap.HEAPU8[ptr + capacity - 1] = 1; },
      (heap, ptr, capacity) => { if (capacity) heap.HEAPU8[ptr] = 0; },
      (heap, ptr, capacity) => { if (capacity) heap.HEAPU8[ptr] = 0xFF; },
      () => 0xFFFFFFFF,
    ]) {
      reset({ stringMutation }); libraryError(name);
    }
    reset({ string: new TextEncoder().encode("\0") }); libraryError(name);
    for (const pass of ["count", "fill"]) {
      reset({ failPass: pass, errorKind: 3, message: "" }); libraryError(name, true);
    }
    reset();
    assert.equal(fixture.moon.newMoonsAfter(2451545, 4096).length, 4096);
    assert.throws(() => fixture.moon.newMoonsAfter(2451545, 4097), RangeError);
    assert.deepEqual(fixture.moon.newMoonsAfter(2451545, 0), []);
    for (const value of [true, "2024", {}, 1.5]) {
      assert.throws(() => fixture.moon.phaseMoments(value, "new"), TypeError);
    }
    for (const value of [NaN, Infinity, 2 ** 31, -(2 ** 31) - 1]) {
      assert.throws(() => fixture.moon.phaseMoments(value, "new"), RangeError);
    }
    for (const namespace of ["sun", "moon"]) {
      for (const [fieldName, value] of [["lon", 360], ["lat", 91], ["lat", NaN], ["r", 0]]) {
        reset({ mutate: (heap, ptr, layout) => {
          const field = layout.fields.find(({ name }) => name === fieldName);
          heap.HEAPF64[(ptr + field.offset) >> 3] = value;
        } });
        libraryError(() => fixture[namespace].apparentGeocentricCoordinate(2451545));
      }
    }
    for (const [action, fieldName, value] of [
      [() => fixture.time.jdeToUt1(2451545), "month", 13],
      [() => fixture.time.jdeToUt1(2451545), "fraction", 1],
      [() => fixture.jieqi.moment(2024, 0), "jq_idx", 1],
      [() => fixture.lunar.fromGregorian("algo3", { year: 2024, month: 1, day: 1 }), "is_leap", 2],
      [() => fixture.sun.longitudeCrossings(2024, 0), "count", 3],
    ]) {
      reset({ mutate: (heap, ptr, layout) => {
        const field = layout.fields.find(({ name }) => name === fieldName);
        if (field.type === "double") heap.HEAPF64[(ptr + field.offset) >> 3] = value;
        else if (["bool", "uint8_t"].includes(field.type)) heap.HEAPU8[ptr + field.offset] = value;
        else heap.HEAPU32[(ptr + field.offset) >> 2] = value;
      } });
      libraryError(action);
    }
    reset();
    const originalError = console.error;
    try {
      console.error = () => assert.fail("binding must not echo native errors");
      control.failPass = "sret"; control.errorKind = 2; control.message = "native failure";
      libraryError(scalar, true);
    } finally { console.error = originalError; }
    reset();
    fixture.lunar.yearInfo("algo3", 1500);
    assert.deepEqual(control.calls, [["celestial_get_lunar_year_info", 3, 1500]], "native owns lunar domains");
    reset();
    fixture.time.deltaT(2035, "algo4");
    assert.deepEqual(control.calls, [["celestial_delta_t", 2035, 4]], "native owns model domains");
    reset();
    fixture.time.ut1ToJd({ ...civil, year: 67562, month: 257 });
    assert.deepEqual(control.calls, [["celestial_ut1_to_jd", 67562, 257, 1, 0.5]], "full-width civil values reach native validation");
    assert.equal(control.live.size, 0);
    console.log("PASS fake native: loader retry; error kinds/empty messages; count/fill errors/totals; 8 allocation-failure paths; sized UTF-8; malformed successes; FFI widths/resource caps");
  } finally {
    delete globalThis.__celestialFakeFactory;
    delete globalThis.__celestialFakeLoad;
    await rm(directory, { recursive: true, force: true });
  }
}
