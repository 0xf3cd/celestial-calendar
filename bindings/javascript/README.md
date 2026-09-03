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
const lichun = celestial.jieqi.moment(2026, 0);
console.log(moon.fraction, lichun, celestial.jieqi.name(0));
```

Concurrent and repeated `init()` calls share one initialization. If loading fails, a later explicit call may retry.

## API

The package exposes `config`, `time`, `sun`, `moon`, `jieqi`, and `lunar` namespaces. TypeScript declarations ship
with the package; enum-like inputs are string unions such as `"full"`, `"algo3"`, and `"debug"`.

Time scales and units stay explicit:

- JD inputs and outputs are named as UT1 or JDE (TT) by the operation.
- Jieqi moments are UT1 civil moments.
- `sun.apparentSolarTime()` accepts civil UTC and east-positive longitude.
- Angular results use degrees; Sun distance uses AU and Moon distance uses kilometres.
- `time.deltaT()` returns seconds.
- The equation of time is degrees of hour angle; multiply by 240 for seconds of time.

`jieqi.moment(year, index)` accepts Gregorian years in `[401, 32766]` and Jieqi indices in `[0, 23]`.
`sun.longitudeCrossings(year, longitudeDeg)`, `moon.phaseMoments(year, phase)`, and `moon.newMoonsInYear(year)` accept
Gregorian years in `[1, 32766]`.
Lunar conversions use algorithm-specific year windows; query them with `lunar.supportedYearRange(algorithm)`.

`time.deltaT(year, model)` accepts a finite decimal Gregorian year. Three models have additional bounds:

| Model | Year domain |
|---|---|
| `"algo1"` | `year >= -4000` |
| `"algo3"` | `year < 3000` |
| `"algo4"` | `year < 2035` |

`"default"`, `"algo2"`, and `"algo5"` have no model-specific year bound.

JavaScript `Date` is intentionally not accepted because it represents UTC milliseconds, not UT1 or TT.

Bad shapes and types throw `TypeError`; JavaScript range guards throw `RangeError`. Native failures throw
`CelestialError`, whose `operation` names the public method and whose `recorded` flag says whether the message came
from the native error channel. A legitimate absence remains `null` or `[]`.

`moon.newMoonsAfter(jde, count)` accepts `count` in `[0, 4096]`; zero returns `[]`. The upper bound keeps the WASM
output buffer at or below 32 KiB, matching the Python package.

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

## License

Project-authored package material is licensed under MIT.

Bundled third-party components retain their own terms in `THIRD_PARTY_NOTICES.txt`.
