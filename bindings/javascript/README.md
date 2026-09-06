<!--
  CelestialCalendar:
    A C++23-style library that performs astronomical calculations and date conversions among various calendars,
    including Gregorian, Lunar, and Chinese Ganzhi calendars.

  Copyright (C) 2026 Ningqi Wang (0xf3cd)
  Email: nq.maigre@gmail.com
  Repo : https://github.com/0xf3cd/celestial-calendar

  SPDX-License-Identifier: MIT
-->

# @0xf3cd/celestial

Astronomical calculations and Chinese calendar conversion from
[CelestialCalendar](https://github.com/0xf3cd/celestial-calendar), distributed as one ESM package with its own
WebAssembly module.

## Install

Install the package from npm:

```sh
npm install @0xf3cd/celestial
```

The matching [GitHub release](https://github.com/0xf3cd/celestial-calendar/releases) also carries
`celestial-wasm.zip`, including the exact tarball published to npm and its SHA-256 sidecar.

Node 22 or newer is supported. The browser package is tested on Chrome. Importing the package performs no I/O;
call `init()` once before using the synchronous calculation APIs.

```js
import * as celestial from "@0xf3cd/celestial";

await celestial.init();

const moon = celestial.moon.illumination(2448724.5);
const lichun = celestial.jieqi.moment(2026, celestial.Jieqi.LICHUN);
console.log(moon.fraction, lichun.momentUt1, celestial.jieqi.name(lichun.jieqi));
```

Concurrent and repeated `init()` calls share one initialization. If loading fails, a later explicit call may retry.

## API

The root entry exposes `config`, `time`, `sun`, `moon`, `jieqi`, and `lunar` namespaces. TypeScript declarations ship
with the package. Model, phase, and logging choices are string unions such as `"full"`, `"algo3"`, and `"debug"`.
`Jieqi` is a frozen object with 24 named constants, from `Jieqi.LICHUN = 0` through `Jieqi.DAHAN = 23`, using the
same spellings as the Python package. The TypeScript type `Jieqi` is the corresponding `0 | 1 | ... | 23` union;
a general `number` variable must be narrowed before passing it to `jieqi.moment()` or `jieqi.name()`.

Time scales and units stay explicit:

- JD inputs and outputs are named as UT1 or JDE (TT) by the operation.
- `jieqi.moment()` returns `{ jieqi, momentUt1 }`. Its nested civil moment is UT1, not UTC or an east-eight wall
  clock; rendering the same instant at UTC+8 can change its calendar date. Establish the time-scale conversion
  before using a UTC or fixed-offset display.
- `sun.apparentSolarTime()` accepts civil UTC and east-positive longitude.
- Angular results use degrees; Sun distance uses AU and Moon distance uses kilometres.
- `time.deltaT()` returns seconds.
- The equation of time is degrees of hour angle; multiply by 240 for seconds of time.
- `sun.apparentGeocentricCoordinate(jde)` and `moon.apparentGeocentricCoordinate(jde)` return one coordinate record
  each at a TT-based JDE.

`jieqi.moment(year, index)` accepts Gregorian years in `[401, 32766]` and Jieqi indices in `[0, 23]`.
`sun.longitudeCrossings(year, longitudeDeg)`, `moon.phaseMoments(year, phase)`, and `moon.newMoonsInYear(year)` accept
Gregorian years in `[1, 32766]`.

Bad shapes and types throw `TypeError`; JavaScript range guards throw `RangeError`. Native failures throw
`CelestialError`, whose `operation` names the public method and whose `recorded` flag says whether the message came
from the native error channel. A legitimate absence remains `null` or `[]`.

`moon.newMoonsAfter(jde, count)` accepts `count` in `[0, 4096]`; zero returns `[]`. The upper bound keeps the WASM
output buffer at or below 32 KiB, matching the Python package.

### Date records

The three input kinds are disjoint, both in TypeScript and at runtime:

| Type | Required own fields | Excluded fields |
|---|---|---|
| `GregorianDate` | `year`, `month`, `day` | `fraction`, `isLeap` |
| `CivilDateTime` | `year`, `month`, `day`, `fraction` | `isLeap` |
| `LunarDate` | `year`, `month`, `day`, `isLeap` | `fraction` |

Civil and Gregorian years are integers in `[1, 32767]`, with valid Gregorian month/day fields. Lunar years and
dates must exist in the selected algorithm. `fraction` is a finite day fraction in `[0, 1)`. Unrelated extra
properties are allowed at runtime, but excluded fields are rejected even when inherited or set to `undefined`.
Enable TypeScript's `exactOptionalPropertyTypes` to reject explicit `undefined` on the optional `never` fields too.

`CivilDateTimeResult` extends `CivilDateTime` with integer `hour` and `minute` and fractional `second`.
`time.jdeToUt1()`, `sun.apparentSolarTime()`, and `JieqiMoment.momentUt1` use this richer output. These fields are
derived from the day fraction without rounding to milliseconds; `second` is seconds, not a millisecond count.
Results remain valid civil inputs: functions read `fraction`, not the derived clock fields. Time scales are named
by operations, not encoded in this shared record type.

Record inputs use structural typing: the explicit own fields and excluded tags determine their kind, not their
constructor. No `Date` methods or timestamps are read. An ordinary `Date` lacks the required fields; use the
date bridge below to render its timestamp as civil fields. A civil result or lunar date cannot be passed
directly to `lunar.fromGregorian()`; select a date basis first, then construct a `GregorianDate` explicitly.

### Fixed-offset Date bridge

`@0xf3cd/celestial/date` is a pure calendar bridge with no runtime dependencies. It does not import the root entry
or load WASM, and works without `init()`:

```js
import { civilAtOffsetToDate, dateToCivilAtOffset } from "@0xf3cd/celestial/date";

const date = new Date("2024-02-09T16:00:00.123Z");
const eastEight = dateToCivilAtOffset(date, 480);
// Local civil date: 2024-02-10, 00:00:00.123.
console.log(eastEight.year, eastEight.month, eastEight.day, eastEight.second);
console.log(civilAtOffsetToDate(eastEight, 480).getTime() === date.getTime()); // true
```

| Function | Input | Output |
|---|---|---|
| `dateToCivilUtc(date)` | `Date` | `CivilDateTimeResult` in UTC |
| `civilUtcToDate(civil)` | `CivilDateTime` in UTC | `Date` |
| `dateToCivilAtOffset(date, offsetMinutesEast)` | `Date`, fixed offset | `CivilDateTimeResult` at that offset |
| `civilAtOffsetToDate(civil, offsetMinutesEast)` | `CivilDateTime` at the fixed offset | `Date` |

`offsetMinutesEast` is a safe integer in `[-1439, 1439]`, positive east of UTC. Conversion uses epoch arithmetic
and UTC fields, not the host's local zone, `Intl`, locale parsing, IANA zones, or daylight-saving rules.

Date-to-civil conversion preserves the host value's millisecond resolution; its fractional `second` has no extra
sub-millisecond information. Civil-to-Date conversion rounds the non-negative local time of day to the nearest
millisecond, with exact half milliseconds toward the next civil instant and explicit carry into the next day.
For any representable `Date` whose civil year at the chosen offset is in `[1, 32767]`, a same-offset
`Date -> civil -> Date` round trip preserves `getTime()` exactly.

The year bound applies to the local civil record, not always the UTC carrier. Both the input and the rounded,
carry-adjusted local year must remain in `[1, 32767]`; rounding the end of year 32767 into local year 32768 is
rejected. At offset edges a returned UTC `Date` may have year 0 or 32768, and can be converted back with the same
offset. The UTC-only functions apply the bound to UTC civil fields.

Wrong types, missing civil fields, mixed date kinds, and non-integer date/offset fields throw `TypeError`.
Invalid `Date` values, invalid Gregorian dates, non-finite/unsafe numbers, out-of-range fractions or offsets, and
out-of-domain local years throw `RangeError`.

This bridge does not convert time scales or account for leap seconds. It provides neither Date-to-JDE nor
UTC-to-UT1 conversion. Do not pass `momentUt1` to `civilUtcToDate()` as though it were UTC: the library does not
model DUT1, and no full-domain `UTC == UT1` approximation is promised.

### Lunar algorithms

Gregorian inputs and outputs are calendar-date labels on the selected algorithm's basis, not instants.
Choose the source and compatibility role you need, not an assumed accuracy ranking:

| Algorithm | Source and status | Lunar-year domain | Selection role |
|---|---|---|---|
| `"algo1"` | Published Hong Kong Observatory date labels, stored as a table | `[1901, 2099]` | HKO almanac compatibility |
| `"algo2"` | Computed from VSOP87D and truncated ELP2000-82B | `[410, 2500]` | Astronomical construction over the wider computed window |
| `"algo3"` | Baked hybrid: algo1/HKO in 1901-2099, algo2-generated dates elsewhere | `[1600, 2199]` | Table lookup with HKO compatibility in the overlapping years |

Algo2 assigns dates by rendering TT moments through the library's UTC model and then applying a fixed eight-hour
eastward offset. Before 1972, that model deliberately uses UT1 as a proxy because historical UTC is not modelled.
From 1972 it uses the leap-second table; after the final entry (2017) it holds Delta AT at 37 seconds. Algo1
preserves HKO's published labels rather than imposing this rule on their history; algo3 inherits the basis of each
source slice. The 2500 ceiling is the computed algorithm's enforced civil-date domain, not a physical limit of
VSOP87D or ELP2000-82B.

`lunar.supportedYearRange(algorithm)` queries the native inclusive **lunar-year** range. These are not Gregorian
January-to-December bounds. `lunar.yearInfo(algorithm, year)` returns the Gregorian `firstDay`, a traditional
`leapMonth` number or `null`, and `monthLengths` in calendar order, with the leap month after its ordinary namesake.
`lunar.toGregorian()` takes traditional month numbers `[1, 12]` and an explicit `isLeap` boolean, not a positional
month index. `yearInfo()` and `toGregorian()` query the native range once per call before validating the lunar year;
the JavaScript package keeps no duplicate range table or cache.

All four lunar methods require `await init()`; a call before initialization throws `Error`. After initialization:

- `TypeError` means a wrong input type, missing own field, excluded date-kind tag, non-integer date field, or
  non-boolean `isLeap`.
- `RangeError` means an unknown algorithm, non-finite/unsafe number, invalid Gregorian date (including a year
  outside `[1, 32767]`), a lunar year outside the native range, or lunar month/day outside `[1, 12]`/`[1, 30]`.
- `CelestialError` means a native failure, including a Gregorian label outside the selected algorithm's coverage
  or a lunar leap-month/day combination that does not exist. A native range-query failure also uses this class,
  with `operation` naming the caller, such as `"lunar.yearInfo"`, not the internal query.

For example, `lunar.fromGregorian("algo1", { year: 1900, month: 1, day: 1 })` passes Gregorian validation but throws
`CelestialError` because algo1 cannot represent it. `lunar.yearInfo("algo1", 1900)` instead throws `RangeError`.

### Delta T models

`time.deltaT(year, model)` returns TT minus UT1 in seconds for a finite decimal Gregorian year.
The default is the current project model; the named older models allow historical comparison:

| Model | Source and status | Hard year domain | Selection role |
|---|---|---|---|
| `"default"`, `"algo5"` | Current project model: algo2 before 2005, IERS Bulletin A fit through about 2026.41, then anchored Morrison et al. (2021) long-term extrapolation | No model-specific bound | Current project calculations |
| `"algo1"` | Xu Jianwei's 2008 model, frozen | `year >= -4000` | Comparison with that historical model |
| `"algo2"` | Espenak and Meeus, NASA/TP-2006-214141, frozen | No model-specific bound | NASA polynomial comparison; live pre-2005 branch of algo3, algo4, and algo5 |
| `"algo3"` | Fred Espenak's 2014 eclipse canon, frozen | `year < 3000` | Comparison with that later polynomial model |
| `"algo4"` | IERS Bulletin A observations and USNO predictions, frozen | `year < 2035` | Comparison with the previous project fit/prediction model |

"Frozen" describes model maintenance, not dead code: algo2 still supplies the pre-2005 values used by the current
default. A hard domain is an input contract, not an accuracy claim. Fitted residuals and test tolerances are not
statistical error guarantees, and an unbounded model does not promise useful accuracy at arbitrary years.

## 中文

`@0xf3cd/celestial` 把 CelestialCalendar 的天文计算与公历/阴历转换包装为一个自带 WebAssembly 的
ESM 包。Node 需要 22 或更新版本；浏览器端在 Chrome 上测试。

```js
import * as celestial from "@0xf3cd/celestial";

await celestial.init();
const result = celestial.lunar.fromGregorian("algo3", { year: 2026, month: 8, day: 15 });
```

包只提供 `jieqi` / `lunar` 这一套正式命名，不另设 `solarTerms` / `lunarCalendar` 别名。时间尺度、
经度符号与单位见上面的 API 契约；不要把 JavaScript `Date` 隐式当作 UT1 或 TT。
`@0xf3cd/celestial/date` 可在不调用 `init()` 的情况下转换 UTC 或固定偏移下的民用时间，
但不做 UT1/TT 时标转换。节气结果的 `momentUt1` 是 UT1，不是东八区日期；阴历转换的日期基准取决于所选算法。

## License

Project-authored package material is licensed under MIT.

Bundled third-party components retain their own terms in `THIRD_PARTY_NOTICES.txt`.
