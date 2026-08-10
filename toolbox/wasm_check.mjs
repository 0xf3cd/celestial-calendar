// WASM 闸门(#163):对编译产物跑 golden 数据集 + sret 布局 + 异常路径 + 体积上限。
// 手动腿与 CI 腿共用:node toolbox/wasm_check.mjs(先跑 toolbox/build_wasm.py)。
//
// 两个上限就是门禁本身,动它们要在 PR 里说清理由:
//   MAX_FRAC_ULP   —— frac 的 bit 距离上限。golden 由 native 生成,与 wasm 的 musl libm
//                    可有 1-ULP 级三角函数差(#163 实测 208 ULP,macOS native ↔ wasm);
//                    取 10× 余量。年/月/日必须全等,不在此宽容内。
//   MAX_WASM_BYTES —— web 变体 .wasm 裸字节上限(2026-08-10 实测 381,647 @ -Oz,
//                    emsdk 6.0.6)。涨过线 = 有人的导出/依赖把体积喂大了,要解释。

import { readFile } from 'node:fs/promises';
import { pathToFileURL } from 'node:url';
import { statSync } from 'node:fs';

const MAX_FRAC_ULP = 2048;
const MAX_WASM_BYTES = 420_000;

const HERE = new URL('.', import.meta.url);
const MODULE_URL = new URL('../build/wasm/celestial-jieqi-node.mjs', HERE);
const GOLDEN_URL = new URL('./wasm_golden.json', HERE);
const WEB_WASM = new URL('../build/wasm/celestial-jieqi-web.wasm', HERE);

let failures = 0;
const check = (ok, label, detail = '') => {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${label}${detail ? '  (' + detail + ')' : ''}`);
  if (!ok) failures++;
};

const M = await (await import(MODULE_URL)).default();
const golden = JSON.parse(await readFile(GOLDEN_URL, 'utf8'));
if (golden.schema !== 'celestial-calendar/wasm-golden@1') {
  throw new Error(`不认得的 golden schema:${golden.schema}`);
}

// 结构体走 sret ABI:调用方 malloc 24B 作首参,按偏移读堆(与 jieqi_table.py 的
// ctypes 镜同一张表)。
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

// 1) sret 布局冒烟:2026 处暑(idx 13)= 2026-08-23
const smoke = query(2026, 13);
check(smoke.valid && smoke.jq === 13 && smoke.y === 2026 && smoke.m === 8 && smoke.d === 23,
  'sret 布局冒烟(2026 处暑 = 2026-08-23)', JSON.stringify(smoke));

// 2) golden 对拍:年月日全等,frac 的 ULP 距离不越上限
const f64 = new Float64Array(1);
const u64 = new BigUint64Array(f64.buffer);
const bitsOf = (x) => { f64[0] = x; return u64[0]; };
let worst = { ulp: -1n };
let bad = 0;
for (const g of golden.entries) {
  const w = query(g.year, g.idx);
  if (!w.valid || w.jq !== g.idx || w.y !== g.y || w.m !== g.m || w.d !== g.d) {
    bad++; continue;
  }
  const a = bitsOf(w.frac), b = BigInt(g.frac_bits);
  const ulp = a > b ? a - b : b - a;
  if (ulp > worst.ulp) worst = { ulp, g };
  if (ulp > BigInt(MAX_FRAC_ULP)) bad++;
}
check(bad === 0, `golden 对拍 ${golden.entries.length} 点(年月日全等 + frac ≤ ${MAX_FRAC_ULP} ULP)`,
  `最大 ULP 距离 ${worst.ulp},不一致 ${bad}`);

// 3) 异常路径:非法 jq_idx 必须 valid=false,且 module 存活
const badIdx = query(2026, 200);
const after = query(2026, 13);
check(!badIdx.valid && after.valid && after.y === 2026,
  '异常路径(idx=200 → valid=false,module 不 trap)');

// 4) get_jieqi_name
const buf = M._malloc(16);
M._get_jieqi_name(13, buf, 16);
const bytes = M.HEAPU8.subarray(buf, buf + 16);
check(new TextDecoder().decode(bytes.slice(0, bytes.indexOf(0))) === '处暑',
  'get_jieqi_name(13) = 处暑');
M._free(buf);
M._free(ptr);

// 5) 体积上限(web 变体裸字节)
const size = statSync(WEB_WASM).size;
check(size <= MAX_WASM_BYTES, `体积 ${size} ≤ ${MAX_WASM_BYTES} bytes`);

if (failures > 0) {
  console.error(`wasm_check: ${failures} 项红`);
  process.exit(1);
}
console.log('wasm_check: 全绿');
