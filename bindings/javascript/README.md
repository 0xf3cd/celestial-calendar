<!--
  CelestialCalendar:
    A C++23-style library that performs astronomical calculations and date conversions between
    Gregorian and Chinese Lunar calendars.

  Copyright (C) 2026 Ningqi Wang (0xf3cd)
  Email: nq.maigre@gmail.com
  Repo : https://github.com/0xf3cd/celestial-calendar

  SPDX-License-Identifier: MIT
-->

# @0xf3cd/celestial

来自 [CelestialCalendar](https://github.com/0xf3cd/celestial-calendar) 的天文计算与公历/阴历转换，
以自带 WebAssembly 模块的 ESM 包提供。

[English guide](https://github.com/0xf3cd/celestial-calendar/blob/main/README_EN.md)

本文对应当前 `0.7.0` 源码。npm 上的版本可能滞后；以下安装命令获取已发布版本，不表示 `0.7.0` 已发布。
使用示例前请核对安装版本是否提供相应 API。

## 安装

在应用项目目录中从 npm 安装：

```sh
npm install @0xf3cd/celestial
```

正式发布时，对应的 [GitHub Release](https://github.com/0xf3cd/celestial-calendar/releases) 还提供
`celestial-wasm.zip`，其中包含与 npm 发布内容逐字节相同的 tarball 及其 SHA-256 校验文件。

支持 Node 22 或更新版本；浏览器端在 Chrome 和 Firefox 上测试。导入包不会进行 I/O；
使用同步计算 API 前，先调用一次 `init()` 并等待完成。

以下每个 JavaScript 代码块均可独立作为应用项目中的 `example.mjs`，从该目录用 `node example.mjs` 运行。

## 示例

### 今天的阴历日期

先取当前时刻在固定 UTC+8 下的公历日期，再按所选算法转换为阴历；这里的“今天”不取决于主机时区。

```js
import * as celestial from "@0xf3cd/celestial";
import { dateToCivilAtOffset } from "@0xf3cd/celestial/date";

const eastEight = dateToCivilAtOffset(new Date(), 480);
const { year, month, day } = eastEight;
const gregorian = { year, month, day };

await celestial.init();
const lunar = celestial.lunar.fromGregorian("algo3", gregorian);
console.log("UTC+8 公历日期：", gregorian);
console.log("阴历日期：", lunar);
```

只选取 `year`、`month`、`day`，不把带 `fraction` 的民用时刻直接传给阴历 API。
日期桥接不把时钟 UTC 转成 UT1 或 TT；阴历日期基准与算法年域见下文。

### 月面照明与节气时刻

`2448724.5` 是 TT 下的 JDE。节气结果的 `momentUt1` 则是 UT1 民用时刻，不是 UTC+8 显示时间。

```js
import * as celestial from "@0xf3cd/celestial";

await celestial.init();

const moon = celestial.moon.illumination(2448724.5);
const lichun = celestial.jieqi.moment(2026, celestial.Jieqi.LICHUN);
console.log(moon.fraction, lichun.momentUt1, celestial.jieqi.name(lichun.jieqi));
```

### 公历与阴历往返

```js
import * as celestial from "@0xf3cd/celestial";

await celestial.init();
const result = celestial.lunar.fromGregorian("algo3", { year: 2026, month: 8, day: 15 });
console.log(result);
console.log(celestial.lunar.toGregorian("algo3", result));
```

## 初始化与浏览器部署

并发和重复的 `init()` 调用共享同一个 promise，完成值为 `undefined`。加载失败时，promise 以原始加载器错误
拒绝；之后可显式再次调用以重试。初始化完成前调用命名空间方法，会在参数校验之前抛出 `CelestialError`，
其 `operation` 为公开方法名，`recorded === false`；即使请求数量为零也一样。

浏览器部署时，确保包的 `.wasm` 资源在构建产物指定的 URL 上可访问，并将静态服务器的响应类型设为
`Content-Type: application/wasm`。

## API 契约

根入口提供 `config`、`time`、`sun`、`moon`、`jieqi`、`lunar` 命名空间，并随包提供 TypeScript 声明。
包只提供 `jieqi` / `lunar` 这一套正式命名，不另设 `solarTerms` / `lunarCalendar` 别名。
模型、月相和日志选项使用字符串联合类型，如 `"full"`、`"algo3"`、`"debug"`。
`Jieqi` 是冻结对象，包含 24 个具名常量，从 `Jieqi.LICHUN = 0` 到 `Jieqi.DAHAN = 23`，拼写与 Python 包一致。
TypeScript 类型 `Jieqi` 是相应的 `0 | 1 | ... | 23` 联合；一般的 `number` 变量须先收窄类型，
才能传给 `jieqi.moment()` 或 `jieqi.name()`。`jieqi.name()` 返回中文名，如 `Jieqi.LICHUN` 对应 `"立春"`。

时间尺度与单位须明确区分：

- JD 输入和输出的操作名区分 UT1 与 JDE（TT）。
- `jieqi.moment()` 返回 `{ jieqi, momentUt1 }`。其中的民用时刻是 UT1，不是 UTC 或东八区钟表时间；
  同一瞬间显示为 UTC+8 时，日期可能改变。显示为 UTC 或固定偏移时间前，须先确定时标转换。
  返回的 UT1 年份可能与请求年份不同。
- `sun.apparentSolarTime()` 接受 UTC 民用时刻，以及 `[-180, 180]` 度范围内、东正西负的地理经度。
- 角度结果以度为单位；日地距离使用 AU，月地距离使用千米。
- `time.deltaT()` 返回秒数。
- 均时差以时角度数表示，乘以 240 可换算为时间秒数。
- `sun.apparentGeocentricCoordinate(jde)` 和 `moon.apparentGeocentricCoordinate(jde)`
  各自返回给定 TT 下 JDE 的一个坐标记录。

`jieqi.moment(year, index)` 接受 `[401, 32766]` 内的公历年份及 `[0, 23]` 内的节气索引。
即使输入在范围内，原生计算无法得到唯一时刻时仍会抛出 `CelestialError`。
`sun.longitudeCrossings(year, longitudeDeg)`、`moon.phaseMoments(year, phase)` 和 `moon.newMoonsInYear(year)`
接受 `[1, 32766]` 内的公历年份。

`time.localApparentSiderealTime(jdUt1, longitudeDeg)` 接受有限的 UT1 下 JD，其公历年份须在 `[401, 32766]` 内；
地理经度须有限、东正西负，范围为 `[-180, 180]` 度。返回值为 `[0, 360)` 度。
年份范围由原生边界校验，因此超出年域的 JD 抛出 `CelestialError`，而非 JavaScript 有限性与经度校验使用的
`RangeError`。

`sun.longitudeCrossings(year, longitudeDeg)` 接受有限的太阳视地心黄经，范围为 `[0, 360)` 度；
返回 TT 下的 JDE 数组，当年没有经过该黄经的时刻时返回 `[]`。

初始化完成后，错误的结构和类型抛出 `TypeError`；JavaScript 范围校验抛出 `RangeError`。
原生失败抛出 `CelestialError`，其 `operation` 为公开方法名，`recorded` 表明消息是否来自原生错误通道。
合法的“无结果”仍以 `null` 或 `[]` 表示。

`moon.newMoonsAfter(jde, count)` 接受 `[0, 4096]` 内的 `count`，零返回 `[]`。
上限使 WASM 输出缓冲区不超过 32 KiB。

### 日期记录

以下三种输入在 TypeScript 与运行时均互斥：

| 类型 | 必需的自有字段 | 排除字段 |
|---|---|---|
| `GregorianDate` | `year`, `month`, `day` | `fraction`, `isLeap` |
| `CivilDateTime` | `year`, `month`, `day`, `fraction` | `isLeap` |
| `LunarDate` | `year`, `month`, `day`, `isLeap` | `fraction` |

民用时刻与公历日期的年份须为 `[1, 32767]` 内的整数，月、日须为有效公历日期；阴历年份和日期须在所选算法中存在。
`fraction` 是 `[0, 1)` 内的有限日小数。运行时允许无关的额外属性，但排除字段即使来自继承或值为 `undefined`
也会被拒绝。启用 TypeScript 的 `exactOptionalPropertyTypes`，也可在类型检查时拒绝可选 `never` 字段上的
显式 `undefined`。

`CivilDateTimeResult` 在 `CivilDateTime` 上增加整数 `hour`、`minute` 及可带小数的 `second`。
`time.jdeToUt1()`、`sun.apparentSolarTime()` 和 `JieqiMoment.momentUt1` 使用这种输出。
这些字段从日小数推导，不舍入到毫秒；`second` 的单位是秒，不是毫秒计数。
结果仍可作为民用时刻输入：函数读取 `fraction`，而非派生的时分秒字段。
时间尺度由操作命名区分，不编码在这个共用记录类型中。

记录输入按结构识别：显式自有字段与排除标签决定种类，而非构造函数；不会读取 `Date` 方法或时间戳。
普通 `Date` 缺少必需字段，须用下文的日期桥接将时间戳转换为民用字段。
民用时刻结果或阴历日期不能直接传给 `lunar.fromGregorian()`；先选定日期基准，再显式构造 `GregorianDate`。

### 固定偏移的 Date 桥接

`@0xf3cd/celestial/date` 是无运行时依赖的纯历法桥接，不导入根入口、不加载 WASM，无需调用 `init()`：

```js
import { civilAtOffsetToDate, dateToCivilAtOffset } from "@0xf3cd/celestial/date";

const date = new Date("2024-02-09T16:00:00.123Z");
const eastEight = dateToCivilAtOffset(date, 480);
// Local civil date: 2024-02-10, 00:00:00.123.
console.log(eastEight.year, eastEight.month, eastEight.day, eastEight.second);
console.log(civilAtOffsetToDate(eastEight, 480).getTime() === date.getTime()); // true
```

| 函数 | 输入 | 输出 |
|---|---|---|
| `dateToCivilUtc(date)` | `Date` | UTC 下的 `CivilDateTimeResult` |
| `civilUtcToDate(civil)` | UTC 下的 `CivilDateTime` | `Date` |
| `dateToCivilAtOffset(date, offsetMinutesEast)` | `Date`、固定偏移 | 该偏移下的 `CivilDateTimeResult` |
| `civilAtOffsetToDate(civil, offsetMinutesEast)` | 固定偏移下的 `CivilDateTime` | `Date` |

`offsetMinutesEast` 是 `[-1439, 1439]` 内的安全整数，以分钟为单位，UTC 以东为正。
转换使用纪元时间运算与 UTC 字段，不使用主机本地时区、`Intl`、区域格式解析、IANA 时区或夏令时规则。

`Date` 转民用时刻保留主机值的毫秒分辨率，带小数的 `second` 不含额外的亚毫秒信息。
民用时刻转 `Date` 时，将非负的本地日内时间舍入到最近的毫秒，恰好半毫秒时向较晚的民用时刻舍入，并显式处理跨日进位。
对任何可表示的 `Date`，只要所选偏移下的民用年份在 `[1, 32767]` 内，使用同一偏移的
`Date -> civil -> Date` 往返就精确保留 `getTime()`。

年份边界约束本地民用记录，不一定约束承载它的 UTC 值。输入与舍入进位后的本地年份都须在 `[1, 32767]` 内；
将本地 32767 年末舍入到 32768 年会被拒绝。在偏移边缘，返回的 UTC `Date` 年份可能为 0 或 32768，
仍可使用同一偏移转回。仅处理 UTC 的函数将年域约束施加于 UTC 民用字段。

错误类型、缺少民用字段、混合日期种类，以及非整数的日期或偏移字段，均抛出 `TypeError`。
无效 `Date`、无效公历日期、非有限数或非安全数、越界的日小数或偏移，以及超出年域的本地年份，均抛出 `RangeError`。

此桥接不转换时间尺度，也不处理闰秒；既不提供 Date 到 JDE 的转换，也不提供 UTC 到 UT1 的转换。
不要把 `momentUt1` 当成 UTC 传给 `civilUtcToDate()`：本库没有 DUT1 模型，也不承诺全域适用的 `UTC == UT1` 近似。

### 阴历算法

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

`lunar.supportedYearRange(algorithm)` 查询原生算法包含两端的**阴历年**范围，而非公历一月至十二月的边界。
`lunar.yearInfo(algorithm, year)` 返回公历 `firstDay`、传统 `leapMonth` 月号或 `null`，
以及按历法顺序排列的 `monthLengths`，闰月紧随同名普通月份。
`lunar.toGregorian()` 使用 `[1, 12]` 内的传统月号和显式 `isLeap` 布尔值，不使用月份的位置索引。
`yearInfo()` 和 `toGregorian()` 每次调用都先查询一次原生范围，再校验阴历年；JavaScript 包不另存范围表或缓存。

四个阴历方法都要求先 `await init()`；初始化完成前调用，会在参数校验之前抛出
`recorded === false` 的 `CelestialError`。初始化完成后：

- `TypeError` 表示输入类型错误、缺少自有字段、带有排除的日期种类标签、日期字段非整数，或 `isLeap` 非布尔值。
- `RangeError` 表示未知算法、非有限数或非安全数、无效公历日期（包括年份超出 `[1, 32767]`）、
  阴历年超出原生范围，或阴历月/日超出 `[1, 12]` / `[1, 30]`。
- `CelestialError` 表示原生失败，包括公历日期标签超出所选算法覆盖范围，或阴历闰月/日组合不存在。
  原生范围查询失败也使用此类，`operation` 指向调用者，如 `"lunar.yearInfo"`，而非内部查询。

例如，`lunar.fromGregorian("algo1", { year: 1900, month: 1, day: 1 })` 能通过公历校验，
但 algo1 无法表示该日期，因此抛出 `CelestialError`；`lunar.yearInfo("algo1", 1900)` 则抛出 `RangeError`。

### Delta T 模型

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

## License

Project-authored package material is licensed under MIT.

Bundled third-party components retain their own terms in `THIRD_PARTY_NOTICES.txt`.
