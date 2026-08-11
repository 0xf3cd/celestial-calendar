// Gate for the WASM build (#163): run the golden dataset, the sret layout smoke, the
// exception path, and the size cap against the freshly built module.
// Manual and CI legs share it: `node toolbox/wasm_check.mjs` (after toolbox/build_wasm.py).
//
//#########################################################################################
//
// CelestialCalendar Automation:
//   Python automation scripts for building and testing the CelestialCalendar C++ project.
//
// Author : Ningqi Wang (0xf3cd)
// Email  : nq.maigre@gmail.com
// Repo   : https://github.com/0xf3cd/celestial-calendar
// License: GNU General Public License v3.0
//
// This software is distributed without any warranty.
// See <https://www.gnu.org/licenses/> for more details.

import { readFile } from 'node:fs/promises';
import { statSync } from 'node:fs';

// The caps ARE the gate; changing them needs a reason in the PR.
//   MAX_FRAC_DIFF_DAYS — absolute tolerance on frac, in days. The golden is
//                    native-generated, and the wasm build links musl's libm, whose trig
//                    can differ from a native libm's, so the solver's final iterate lands
//                    on slightly different roundings. Measured shape (#163, 204 points):
//                    ~1e-14 day in the modern era, up to ~2e-9 day (~170 µs) at the era
//                    edges 401/32766 -- a ULP metric distorts this wildly (small frac
//                    values turn sub-µs diffs into 10^7-ULP counts), so the gate is
//                    absolute. 1e-8 day = 0.86 ms, ~5x above the measured worst; the ΔT
//                    models' own uncertainty at those eras is minutes. Year/month/day
//                    must match exactly -- no tolerance there.
//   MAX_VALUE_DIFF_* — same story for the @2 sections. Moon illumination / elongation are
//                    direct evaluations (no solver amplification), cap 1e-9. Sidereal:
//                    the GMST polynomial is libm-free arithmetic, and native arm64
//                    contracts its multiply-add into fma while em++ does not, so the two
//                    builds land one product-ulp apart; inside the declared window the
//                    product stays below 2^32 and the step tops out at 2^-21 deg ≈ 4.77e-7.
//                    Cap 1e-6 deg (≈ 3.6 mas) is ~2x that; the ΔT models' own uncertainty
//                    at those eras swamps it.
//   MAX_WASM_BYTES — raw-byte cap on the .wasm. The size check below prints the current
//                    artifact's bytes every run; crossing the cap means someone's export
//                    or dependency fattened the artifact, and that takes an explanation.
const MAX_FRAC_DIFF_DAYS = 1e-8;
const MAX_VALUE_DIFF_MOON = 1e-9;      // illumination and elongation_deg
const MAX_VALUE_DIFF_SIDEREAL = 1e-6;  // degrees; one product-ulp, see above
const MAX_WASM_BYTES = 420_000;
const MIN_SECTION_ENTRIES = 30; // an empty section must not pass vacuously (#72's shape)

const HERE = new URL('.', import.meta.url);
const MODULE_URL = new URL('../build/wasm/celestial-jieqi.mjs', HERE);
const GOLDEN_URL = new URL('./wasm_golden.json', HERE);
const WASM_URL = new URL('../build/wasm/celestial-jieqi.wasm', HERE);

let failures = 0;
const check = (ok, label, detail = '') => {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${label}${detail ? '  (' + detail + ')' : ''}`);
  if (!ok) failures++;
};

const M = await (await import(MODULE_URL)).default();
const golden = JSON.parse(await readFile(GOLDEN_URL, 'utf8'));
if (golden.schema !== 'celestial-calendar/wasm-golden@2') {
  throw new Error(`unknown golden schema: ${golden.schema}`);
}
const sections = golden.sections;
for (const [name, s] of Object.entries(sections)) {
  check(s.entries.length >= MIN_SECTION_ENTRIES,
    `golden section ${name}: ${s.entries.length} entries >= ${MIN_SECTION_ENTRIES}`);
}

// Doubles travel as IEEE-754 bit patterns (hex strings) -- decoding is exact.
const f64 = new Float64Array(1);
const u64 = new BigUint64Array(f64.buffer);
const bitsOf = (hex) => { u64[0] = BigInt(hex); return f64[0]; };

// Struct returns use the wasm ABI's sret form: the caller passes scratch bytes as the
// first argument and reads the fields out of the heap (same layouts as the ctypes
// mirrors in statistics/common.py).
const ptr24 = M._malloc(24); // JieqiMomentQuery / MoonIllumination / UT1Time (24 B)
const ptr16 = M._malloc(16); // JulianDay / SiderealTime (16 B)

const query = (year, idx) => {
  M._query_jieqi_moment(ptr24, year, idx);
  return {
    valid: M.HEAPU8[ptr24] === 1,
    jq: M.HEAPU8[ptr24 + 1],
    y: M.HEAP32[(ptr24 + 4) >> 2],
    m: M.HEAPU32[(ptr24 + 8) >> 2],
    d: M.HEAPU32[(ptr24 + 12) >> 2],
    frac: M.HEAPF64[(ptr24 + 16) >> 3],
  };
};
const illum = (jde) => {
  M._moon_illumination(ptr24, jde);
  return {
    valid: M.HEAPU8[ptr24] === 1,
    illumination: M.HEAPF64[(ptr24 + 8) >> 3],
    elongation_deg: M.HEAPF64[(ptr24 + 16) >> 3],
  };
};
const last = (jdUt1, lon) => {
  M._local_apparent_sidereal_time(ptr16, jdUt1, lon);
  return { valid: M.HEAPU8[ptr16] === 1, value: M.HEAPF64[(ptr16 + 8) >> 3] };
};

// 1) sret layout smoke: 2026 处暑 (idx 13) = 2026-08-23
const smoke = query(2026, 13);
check(smoke.valid && smoke.jq === 13 && smoke.y === 2026 && smoke.m === 8 && smoke.d === 23,
  'sret layout smoke (2026 处暑 = 2026-08-23)', JSON.stringify(smoke));

// 2) golden replay, jieqi section: year/month/day exactly equal, frac within the cap.
//    Date mismatches and tolerance overflows are different failure shapes -- count them
//    apart and print the first offenders, or a red log says nothing. `exact` is the
//    jieqi-only exact-match predicate; the other sections have none.
const replay = (entries, call, fields, cap, exact = null) => {
  let exactBad = 0, diffBad = 0, worstDiff = -1, worstPoint = null;
  const offenders = [];
  for (const g of entries) {
    const w = call(g);
    if (!w.valid || (exact !== null && !exact(w, g))) {
      exactBad++;
      if (offenders.length < 3) offenders.push(`exact mismatch: golden ${JSON.stringify(g)} vs wasm ${JSON.stringify(w)}`);
      continue;
    }
    const diff = Math.max(...fields.map((name) => Math.abs(w[name] - bitsOf(g[`${name}_bits`]))));
    // NaN never trips `>`: a non-finite field must count as bad outright, not sail through.
    if (!Number.isFinite(diff)) {
      diffBad++;
      if (offenders.length < 3) offenders.push(`non-finite field at ${JSON.stringify(g)} (wasm ${JSON.stringify(w)})`);
      continue;
    }
    if (diff > worstDiff) { worstDiff = diff; worstPoint = g; }
    if (diff > cap) {
      diffBad++;
      if (offenders.length < 3) offenders.push(`diff ${diff} at ${JSON.stringify(g)} (wasm ${JSON.stringify(w)})`);
    }
  }
  return { exactBad, diffBad, worstDiff, worstPoint, offenders };
};

const jq = replay(
  sections.jieqi.entries,
  (g) => query(g.year, g.idx),
  ["frac"],
  MAX_FRAC_DIFF_DAYS,
  (w, g) => w.jq === g.idx && w.y === g.y && w.m === g.m && w.d === g.d,
);
check(jq.exactBad + jq.diffBad === 0,
  `golden replay jieqi ${sections.jieqi.entries.length} pts (date mismatches ${jq.exactBad}, overflows ${jq.diffBad}, max diff ${jq.worstDiff.toExponential(2)} d at ${JSON.stringify(jq.worstPoint)})`);
for (const o of jq.offenders) console.log(`  offender: ${o}`);

// 2b) golden replay, moon section: illumination and elongation_deg within the cap.
const mi = replay(
  sections.moon.entries,
  (g) => illum(bitsOf(g.jde_bits)),
  ["illumination", "elongation_deg"],
  MAX_VALUE_DIFF_MOON,
);
check(mi.exactBad + mi.diffBad === 0,
  `golden replay moon ${sections.moon.entries.length} pts (invalid ${mi.exactBad}, overflows ${mi.diffBad}, max diff ${mi.worstDiff.toExponential(2)} at ${JSON.stringify(mi.worstPoint)})`);
for (const o of mi.offenders) console.log(`  offender: ${o}`);

// 2c) golden replay, sidereal section: value within the cap.
const st = replay(
  sections.sidereal.entries,
  (g) => last(bitsOf(g.jd_ut1_bits), g.longitude),
  ["value"],
  MAX_VALUE_DIFF_SIDEREAL,
);
check(st.exactBad + st.diffBad === 0,
  `golden replay sidereal ${sections.sidereal.entries.length} pts (invalid ${st.exactBad}, overflows ${st.diffBad}, max diff ${st.worstDiff.toExponential(2)} deg at ${JSON.stringify(st.worstPoint)})`);
for (const o of st.offenders) console.log(`  offender: ${o}`);

// 3) input guard: out-of-range jq_idx returns valid=false via the early guard (no throw)
check(!query(2026, 200).valid, 'input guard (idx=200 -> valid=false)');

// 4) exception path: a year outside [1, 32766] THROWS inside the try block -- this is
//    the check that actually exercises -fwasm-exceptions; an idx=200 never reaches it.
const thrown = query(40000, 13);
const alive = query(2026, 13);
check(!thrown.valid && alive.valid && alive.y === 2026,
  'exception path (year=40000 throws -> valid=false, module survives)');

// 4b) exception path, sidereal: jd_ut1 outside the [401, 32766] window throws inside the
//     wrapper (jd_to_ut1's guard) -> valid=false AND the reason is readable (#97 pilot).
const outWindow = last(1000000.0, 0.0);
const errWindow = M.ccall('last_error', 'string', [], []);
const aliveAfter = last(2451545.0, 0.0);
check(!outWindow.valid && errWindow.length > 0 && aliveAfter.valid,
  'exception path sidereal (jd=1e6 throws -> valid=false + last_error, module survives)',
  JSON.stringify(errWindow));

// 4c) sret smoke, moon: Example 48.a (1992-04-12 0h TT) -> k = 0.6786 by the book.
const book = illum(2448724.5);
check(book.valid && Math.abs(book.illumination - 0.6786) < 5e-5,
  'moon sret smoke (Example 48.a: k = 0.6786)', JSON.stringify(book));
check(!illum(Number.NaN).valid, 'moon input guard (NaN -> valid=false)');

// 4d) sret smoke, UT1Time (jde_to_ut1): 2451545.0 is 12:00 TT at J2000, and UT1 trails TT
//     by ΔT ≈ 63.8 s there, so the fraction is 0.5 − 63.8/86400 ≈ 0.49926, not 0.5.
M._jde_to_ut1(ptr24, 2451545.0);
check(
  M.HEAPU8[ptr24] === 1 &&
  M.HEAP32[(ptr24 + 4) >> 2] === 2000 &&
  M.HEAPU32[(ptr24 + 8) >> 2] === 1 &&
  M.HEAPU32[(ptr24 + 12) >> 2] === 1 &&
  Math.abs(M.HEAPF64[(ptr24 + 16) >> 3] - 0.4992609) < 1e-4,
  'jde_to_ut1 sret smoke (2451545.0 -> 2000-01-01, frac ≈ 0.49926 = 0.5 − ΔT)');

// 5) get_jieqi_name
const buf = M._malloc(16);
M._get_jieqi_name(13, buf, 16);
const nameBytes = M.HEAPU8.subarray(buf, buf + 16);
check(new TextDecoder().decode(nameBytes.slice(0, nameBytes.indexOf(0))) === '处暑',
  'get_jieqi_name(13) = 处暑');
M._free(buf);

// 6) JD exports: ut1_to_jd fixed point (J2000), and last_error as the diagnostics
//    channel -- a bad fraction must fail AND say why.
M._ut1_to_jd(ptr16, 2000, 1, 1, 0.5);
check(M.HEAPU8[ptr16] === 1 && M.HEAPF64[(ptr16 + 8) >> 3] === 2451545.0,
  'ut1_to_jd(2000-01-01 12:00) = 2451545.0 exactly');
M._ut1_to_jd(ptr16, 2026, 8, 23, 9.9);
const err = M.ccall('last_error', 'string', [], []);
check(M.HEAPU8[ptr16] === 0 && typeof err === 'string' && err.length > 0,
  'bad fraction -> valid=false + last_error non-empty', JSON.stringify(err));

// 7) size cap (raw bytes of the shipped .wasm)
const size = statSync(WASM_URL).size;
check(size <= MAX_WASM_BYTES, `size ${size} <= ${MAX_WASM_BYTES} bytes`);

M._free(ptr24);
M._free(ptr16);

if (failures > 0) {
  console.error(`wasm_check: ${failures} check(s) red`);
  process.exit(1);
}
console.log('wasm_check: all green');
