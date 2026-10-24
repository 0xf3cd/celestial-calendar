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

[English](README.md)

本文对应 **1.0.0** 的开发接口，尚未发布。以下安装命令取得 npm 上的最新已发布版本，请使用与所装版本匹配的文档。
日志接口移除、太阳距离字段改名和错误类别变化见[迁移指南（英文）](../../docs/MIGRATING_1_0.md)。

<a id="install"></a>
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

<a id="examples"></a>
## 示例

<a id="chart-snapshot"></a>
### 固定时刻的星盘快照

```js
import { init, chart } from "@0xf3cd/celestial";

await init();
const snapshot = chart.snapshot(
  { year: 2026, month: 1, day: 1, fraction: 0.5 },
  {
    scale: "utc",
    location: { latitudeDeg: 51.5, longitudeDeg: 0 },
    system: "placidus",
    model: "default",
  },
);
console.log(snapshot.times); // { jdUt1, jdeTt }
console.log(snapshot.bodies[2]); // Mercury: position, distance and longitude rate
console.log(snapshot.houses.cuspsDeg); // twelve house cusps in house order
```

`civil` 使用公历日期与 `[0, 1)` 的日小数，不自动转换 JavaScript `Date`。`scale` 必填，
可选 `"utc"`、`"ut1"`；1972-01-01 之前的输入必须显式选择 `scale: "ut1"`。民用年份限 1885–2099。
纬度以北为正，单位为度，范围 `(-90, 90)`；经度以东为正，范围 `[-180, 180]`。
`system` 必填，可选 `"equal"`、`"wholeSign"`、`"placidus"`。
`model` 可省略，或设为自身属性 `undefined`、`"default"`，均选 Algo5；也可显式选 `"algo1"` 至
`"algo5"`。使用的输入字段须为对象自身属性；继承来的 `model` 会报错，不会静默换成默认模型。
`options` 只接受 `scale`、`location`、`system`、`model` 四个键；拼错或多余的键抛 `TypeError`。

结果包含 `times: { jdUt1, jdeTt }`，以及固定顺序的十四个 `bodies`，其 `target` 为 `sun`、`moon`、
`mercury`、`venus`、`mars`、`jupiter`、`saturn`、`uranus`、`neptune`、`pluto`、`meanAscending`、
`meanDescending`、`trueAscending`、`trueDescending`。每个成员包含 `longitudeDeg`、`latitudeDeg`、
`distanceAu` 和 `longitudeRateDegPerTtDay`。所有成员均以日期真黄道／真春分点为基准：太阳、月球与行星
提供视地心坐标；交点提供平均／真交点黄经，纬度为零、`distanceAu` 为 `null`。
所有距离都用 AU，月球也一样；独立的 `moon.apparentGeocentricCoordinate()` 则以千米给出月地距离。
`longitudeRateDegPerTtDay` 是瞬时黄经速率，单位为度/TT 日；负值表示该瞬间黄经在减小（逆行）。
`houses` 包含 `ascendantDeg`、`midheavenDeg`、`descendantDeg`、`imumCoeliDeg` 和 `cuspsDeg`，
角度均在 `[0, 360)` 度。结果是独立的普通对象和数组，后续调用或 WASM 内存增长不改变既有结果。

核心检查换算后 JDE(TT) 的星盘范围 `[2409543.875, 2488069.125)`；Algo4 还要求派生
`JDE < 2464328.5`，在反向换算前检查。这会有意排除 Algo4 下 2034 年末约 71.5 个 UT1 秒；
共用民用／模型错误可能先于星盘联合窗口错误出现。由 UTC 换算出的 UT1 是模型值，不是观测 DUT1；
未来 UTC 沿用闰秒表最后已知的偏移。见
[Time scales](../../docs/DESIGN.md#time-scales)。
Ascendant 在舍入尺度上的退化方向、以及 Placidus 的极区情形会失败。宫位遵循宫位操作的章动默认模型，当前为 IAU 1980；
天体位置修正仍固定为 IAU 1980。类型／记录结构错误抛 `TypeError`；无效的日期、日小数、地点、
选项字符串或原生域拒绝抛 `RangeError`；未初始化、内存分配失败、原生计算失败或原生成功记录畸形抛 `CelestialError`，其 `operation` 为
`"chart.snapshot"`。`recorded` 仅在错误消息来自原生错误通道时为 `true`。失败不返回半张 snapshot。

<a id="todays-lunar-date"></a>
### 今天的阴历日期

用 `/date` 取当前时刻在固定 UTC+8 下的公历日期，再按所选算法转换为阴历；这里的“今天”不取决于主机时区。
只取年月日构造公历日期，不要把含有 `fraction` 的整个民用时间对象传给 `lunar.fromGregorian()`。

```js
import * as celestial from "@0xf3cd/celestial";
import { dateToCivilAtOffset } from "@0xf3cd/celestial/date";

await celestial.init();
const { year, month, day } = dateToCivilAtOffset(new Date(), 480);
const gregorian = { year, month, day };
const lunar = celestial.lunar.fromGregorian("algo3", gregorian);
console.log(gregorian, lunar);
```

日期桥接不把时钟 UTC 转成 UT1 或 TT；阴历日期基准与算法年域见[阴历算法](#lunar-algorithms)。

<a id="next-jieqi"></a>
### 下一个节气，还剩多少天

起点是明确给定的 **2026-12-31 00:00 UT1**，不是把 `Date` 时间戳当作 UT1。

```js
import * as celestial from "@0xf3cd/celestial";

await celestial.init();
const cutoffUt1 = { year: 2026, month: 12, day: 31, fraction: 0 };
const cutoffJd = celestial.time.ut1ToJd(cutoffUt1);
const upcoming = [2026, 2027]
  .flatMap((year) => Object.values(celestial.Jieqi).map((jieqi) => {
    const event = celestial.jieqi.moment(year, jieqi);
    return { ...event, jdUt1: celestial.time.ut1ToJd(event.momentUt1) };
  }))
  .filter((event) => event.jdUt1 > cutoffJd)
  .sort((a, b) => a.jdUt1 - b.jdUt1)[0];
if (upcoming === undefined) throw new Error("No later Jieqi in the queried years.");
console.log(celestial.jieqi.name(upcoming.jieqi));
console.log("UT1:", upcoming.momentUt1);
console.log("Remaining UT1 days:", upcoming.jdUt1 - cutoffJd);
```

这里遍历全部枚举值，按时刻而非编号选取下一项，并保留剩余天数的小数部分。
`momentUt1` 是 UT1，不是 UTC 或东八区时间。此例只演示这次跨年查询，不是覆盖整个 API 年域的通用搜索；
`jieqi.moment()` 的查询年域与失败条件见 [API 契约](#api-contract)。

<a id="moon-illumination-and-jieqi-moment"></a>
### 月面照明与节气时刻

`2448724.5` 是 TT 下的 JDE。节气结果的 `momentUt1` 则是 UT1 民用时刻，不是 UTC+8 显示时间。

```js
import * as celestial from "@0xf3cd/celestial";

await celestial.init();

const moon = celestial.moon.illumination(2448724.5);
const lichun = celestial.jieqi.moment(2026, celestial.Jieqi.LICHUN);
console.log(moon.fraction, lichun.momentUt1, celestial.jieqi.name(lichun.jieqi));
```

<a id="gregorian-lunar-round-trip"></a>
### 公历与阴历往返

```js
import * as celestial from "@0xf3cd/celestial";

await celestial.init();
const result = celestial.lunar.fromGregorian("algo3", { year: 2026, month: 8, day: 15 });
console.log(result);
console.log(celestial.lunar.toGregorian("algo3", result));
```

<a id="initialization"></a>
## 初始化与浏览器部署

并发和重复的 `init()` 调用共享同一个 promise，完成值为 `undefined`。加载失败时，promise 以原始加载器错误
拒绝；之后可显式再次调用以重试。初始化完成前调用命名空间方法，会在参数校验之前抛出 `CelestialError`，
其 `operation` 为公开方法名，`recorded === false`；即使请求数量为零也一样。

浏览器部署时，确保包的 `.wasm` 资源在构建产物指定的 URL 上可访问，并将静态服务器的响应类型设为
`Content-Type: application/wasm`。

<a id="api-contract"></a>
## API 契约

根入口提供 `time`、`sun`、`moon`、`jieqi`、`lunar`、`chart` 命名空间，并随包提供 TypeScript 声明。
包只提供 `jieqi` / `lunar` 这一套正式命名，不另设 `solarTerms` / `lunarCalendar` 别名。
模型和月相选项使用字符串联合类型，如 `"full"`、`"algo3"`。不再提供日志配置或 `init` 配置参数。
独立位置、赤道位置、宫位、事件、能力与模型文本查询均不是公开 JavaScript API。
`Jieqi` 是冻结对象，包含 24 个具名常量，从 `Jieqi.LICHUN = 0` 到 `Jieqi.DAHAN = 23`，拼写与 Python 包一致。
TypeScript 类型 `Jieqi` 是相应的 `0 | 1 | ... | 23` 联合；一般的 `number` 变量须先收窄类型，
才能传给 `jieqi.moment()` 或 `jieqi.name()`。`jieqi.name()` 返回中文名，如 `Jieqi.LICHUN` 对应 `"立春"`。

时间尺度与单位须明确区分：

- JD 输入和输出的操作名区分 UT1 与 JDE（TT）。
- `jieqi.moment()` 返回 `{ jieqi, momentUt1 }`，TT 到 UT1 固定使用 Algo5，不随可选模型的默认值改变。
  其中的民用时刻是 UT1，不是 UTC 或东八区钟表时间；
  同一瞬间显示为 UTC+8 时，日期可能改变。显示为 UTC 或固定偏移时间前，须先确定时标转换。
  返回的 UT1 年份可能与请求年份不同。
- `sun.apparentSolarTime()` 接受 UTC 民用时刻，以及 `[-180, 180]` 度范围内、东正西负的地理经度。
- 角度结果以度为单位。日地距离字段为 `distanceAu`，单位为 AU。`moon.apparentGeocentricCoordinate()` 的月地距离用千米；
  `chart.snapshot()` 里所有天体（含月球）的距离都用 AU。
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
年份范围由原生边界校验，超出年域的 JD 与 JavaScript 有限性／经度校验一样抛出 `RangeError`。
此操作的 ΔT 固定为 Algo5，章动固定为 IAU 1980。

`sun.longitudeCrossings(year, longitudeDeg)` 接受有限的太阳视地心黄经，范围为 `[0, 360)` 度；
返回 TT 下的 JDE 数组，当年没有经过该黄经的时刻时返回 `[]`。

初始化完成后，错误的结构和类型抛出 `TypeError`；无效输入或声明域之外的输入，无论在哪一层发现，都抛出 `RangeError`。
计算／资源／内部失败或原生成功记录畸形抛出 `CelestialError`，其 `operation` 为公开方法名，
`recorded` 表明消息是否来自原生错误通道。原生错误种类决定是否失败，即使消息为空也一样。
合法的“无结果”仍以 `null` 或 `[]` 表示。

有限的位置输入仍可能计算失败；坐标合法不保证远年代的物理合理性或精度，不设新的统一位置年域或黄赤交角阈值。
绑定对调用方持有的 WASM 输出缓冲区设有 16 MiB 上限。

`moon.newMoonsAfter(jde, count)` 接受 `[0, 4096]` 内的 `count`，零返回 `[]`。
上限使 WASM 输出缓冲区不超过 32 KiB。

<a id="date-records"></a>
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

<a id="date-bridge"></a>
### 固定偏移的 Date 桥接

`@0xf3cd/celestial/date` 是无运行时依赖的纯历法桥接，不导入根入口、不加载 WASM，无需调用 `init()`。
偏移量以分钟为单位，向东为正；`480` 表示固定 UTC+8。

```js
import { civilAtOffsetToDate, dateToCivilAtOffset } from "@0xf3cd/celestial/date";

const date = new Date("2024-02-09T16:00:00.123Z");
const eastEight = dateToCivilAtOffset(date, 480);
const restored = civilAtOffsetToDate(eastEight, 480);
console.log(eastEight);
console.log(restored.toISOString(), restored.getTime() === date.getTime());
```

这里的 `2024-02-09T16:00:00.123Z` 在 UTC+8 下是民用日期 2024-02-10、00:00:00.123。
同一偏移下往返保留 `Date` 的毫秒时间戳，所以最后一行以 `true` 结尾。

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
不要把 `momentUt1` 当成 UTC 传给 `civilUtcToDate()`：UT1 不是 UTC，本库也不建模观测 DUT1。
两者可能相差多少，见 [Time scales](../../docs/DESIGN.md#time-scales)。

<a id="lunar-algorithms"></a>
### 阴历算法

公历输入与输出都是所选算法日期基准下的日期标签，不是瞬间。各算法的来源、阴历年域、选用场景与日期基准见英文版的
[Lunar Algorithms](README.md#lunar-algorithms)。

`lunar.supportedYearRange(algorithm)` 查询原生算法包含两端的**阴历年**范围，而非公历一月至十二月的边界。
`lunar.yearInfo(algorithm, year)` 返回公历 `firstDay`、传统 `leapMonth` 月号或 `null`，
以及按历法顺序排列的 `monthLengths`，闰月紧随同名普通月份。
`lunar.toGregorian()` 使用 `[1, 12]` 内的传统月号和显式 `isLeap` 布尔值，不使用月份的位置索引。
所选算法的阴历年份和日期由原生操作校验；JavaScript 包不另存范围表或缓存。

四个阴历方法都要求先 `await init()`；初始化完成前调用，会在参数校验之前抛出
`recorded === false` 的 `CelestialError`。初始化完成后：

- `TypeError` 表示输入类型错误、缺少自有字段、带有排除的日期种类标签、日期字段非整数，或 `isLeap` 非布尔值。
- `RangeError` 表示未知算法、非有限数或非安全数、无效公历日期（包括年份超出 `[1, 32767]`）、
  阴历年超出原生范围，或阴历月/日超出 `[1, 12]` / `[1, 30]`。
- `RangeError` 也包括原生无效输入拒绝，如公历日期标签超出所选算法覆盖范围，或阴历闰月/日组合不存在。
- `CelestialError` 表示计算／资源／内部失败或原生成功记录畸形，`operation` 指向调用者，
  如 `"lunar.yearInfo"`，而非内部范围查询。

例如，`lunar.fromGregorian("algo1", { year: 1900, month: 1, day: 1 })` 能通过公历校验，
但 algo1 无法表示该日期，因此抛出 `RangeError`；`lunar.yearInfo("algo1", 1900)` 也抛出 `RangeError`。

<a id="delta-t-models"></a>
### Delta T 模型

`time.deltaT(year, model)` 对有限的十进制公历年返回 TT − UT1 的秒数。各模型的来源、硬年域与选用场景见英文版的
[Delta T Models](README.md#delta-t-models)。

<a id="license"></a>
## 许可证

[许可证范围与第三方例外](README.md#license)以英文版的 License 节为准。
