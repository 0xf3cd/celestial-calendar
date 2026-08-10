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

// The two caps ARE the gate; changing them needs a reason in the PR.
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
//   MAX_WASM_BYTES — raw-byte cap on the .wasm (measured 381,216 with
//                    -Oz -DNDEBUG, emsdk 6.0.6). Crossing it means someone's export or
//                    dependency fattened the artifact; that takes an explanation.
const MAX_FRAC_DIFF_DAYS = 1e-8;
const MAX_WASM_BYTES = 420_000;
const MIN_GOLDEN_ENTRIES = 150; // an empty dataset must not pass vacuously (#72's shape)

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
if (golden.schema !== 'celestial-calendar/wasm-golden@1') {
  throw new Error(`unknown golden schema: ${golden.schema}`);
}
check(golden.entries.length >= MIN_GOLDEN_ENTRIES,
  `golden size ${golden.entries.length} >= ${MIN_GOLDEN_ENTRIES}`);

// Struct returns use the wasm ABI's sret form: the caller passes 24 scratch bytes as the
// first argument and reads the fields out of the heap (same layout as the ctypes mirror
// in jieqi_table.py: valid@0, idx@1, y@4, m@8, d@12, frac@16).
const ptr = M._malloc(24);
const query = (year, idx) => {
  M._query_jieqi_moment(ptr, year, idx);
  return {
    valid: M.HEAPU8[ptr] === 1,
    jq: M.HEAPU8[ptr + 1],
    y: M.HEAP32[(ptr + 4) >> 2],
    m: M.HEAPU32[(ptr + 8) >> 2],
    d: M.HEAPU32[(ptr + 12) >> 2],
    frac: M.HEAPF64[(ptr + 16) >> 3],
  };
};

// 1) sret layout smoke: 2026 处暑 (idx 13) = 2026-08-23
const smoke = query(2026, 13);
check(smoke.valid && smoke.jq === 13 && smoke.y === 2026 && smoke.m === 8 && smoke.d === 23,
  'sret layout smoke (2026 处暑 = 2026-08-23)', JSON.stringify(smoke));

// 2) golden: year/month/day exactly equal, frac absolute difference within the cap.
//    Date mismatches and tolerance overflows are different failure shapes -- count them
//    apart and print the first offenders, or a red log says nothing.
const f64 = new Float64Array(1);
const u64 = new BigUint64Array(f64.buffer);
const fracOf = (hex) => { u64[0] = BigInt(hex); return f64[0]; };
let dateBad = 0, diffBad = 0, worstDiff = -1, worstPoint = null;
const offenders = [];
for (const g of golden.entries) {
  const w = query(g.year, g.idx);
  if (!w.valid || w.jq !== g.idx || w.y !== g.y || w.m !== g.m || w.d !== g.d) {
    dateBad++;
    if (offenders.length < 3) offenders.push(`date mismatch: golden ${JSON.stringify(g)} vs wasm ${JSON.stringify(w)}`);
    continue;
  }
  const diff = Math.abs(w.frac - fracOf(g.frac_bits));
  if (diff > worstDiff) { worstDiff = diff; worstPoint = g; }
  if (diff > MAX_FRAC_DIFF_DAYS) {
    diffBad++;
    if (offenders.length < 3) offenders.push(`diff ${diff} d at ${JSON.stringify(g)} (wasm frac ${w.frac})`);
  }
}
check(dateBad + diffBad === 0,
  `golden replay ${golden.entries.length} pts (date mismatches ${dateBad}, overflows ${diffBad}, max diff ${worstDiff.toExponential(2)} d at ${JSON.stringify(worstPoint)})`);
for (const o of offenders) console.log(`  offender: ${o}`);

// 3) input guard: out-of-range jq_idx returns valid=false via the early guard (no throw)
check(!query(2026, 200).valid, 'input guard (idx=200 -> valid=false)');

// 4) exception path: a year outside [1, 32766] THROWS inside the try block -- this is
//    the check that actually exercises -fwasm-exceptions; an idx=200 never reaches it.
const thrown = query(40000, 13);
const alive = query(2026, 13);
check(!thrown.valid && alive.valid && alive.y === 2026,
  'exception path (year=40000 throws -> valid=false, module survives)');

// 5) get_jieqi_name
const buf = M._malloc(16);
M._get_jieqi_name(13, buf, 16);
const nameBytes = M.HEAPU8.subarray(buf, buf + 16);
check(new TextDecoder().decode(nameBytes.slice(0, nameBytes.indexOf(0))) === '处暑',
  'get_jieqi_name(13) = 处暑');
M._free(buf);

// 6) JD exports: ut1_to_jd fixed point (J2000), and last_error as the diagnostics
//    channel -- a bad fraction must fail AND say why.
const jd = M._malloc(16);
M._ut1_to_jd(jd, 2000, 1, 1, 0.5);
check(M.HEAPU8[jd] === 1 && M.HEAPF64[(jd + 8) >> 3] === 2451545.0,
  'ut1_to_jd(2000-01-01 12:00) = 2451545.0 exactly');
M._ut1_to_jd(jd, 2026, 8, 23, 9.9);
const err = M.ccall('last_error', 'string', [], []);
check(M.HEAPU8[jd] === 0 && typeof err === 'string' && err.length > 0,
  'bad fraction -> valid=false + last_error non-empty', JSON.stringify(err));
M._free(jd);

// 7) size cap (raw bytes of the shipped .wasm)
const size = statSync(WASM_URL).size;
check(size <= MAX_WASM_BYTES, `size ${size} <= ${MAX_WASM_BYTES} bytes`);

M._free(ptr);

if (failures > 0) {
  console.error(`wasm_check: ${failures} check(s) red`);
  process.exit(1);
}
console.log('wasm_check: all green');
