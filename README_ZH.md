# Celestial Calendar

天文计算与公历、阴历转换：查询节气时刻、日月位置、日出日落。核心是 C++23 头文件库，
也可通过 Python、JavaScript / TypeScript 或 C ABI 使用。

[English](README.md) · [Python](#python) · [JavaScript / TypeScript](#javascript) ·
[C++](#cpp) · [C / FFI](#c-abi) · [功能与算法](#features) ·
[构建（英文）](docs/BUILDING.md) · [验证（英文）](docs/VALIDATION.md) · [设计（英文）](docs/DESIGN.md)

本文对应 **0.8.0** 版。下面的安装命令取得 PyPI / npm 上的最新版本；更早的版本可能缺少本文的部分接口，请以所装版本附带的文档为准。
要自行构建当前源码，可按[本机 Python wheel](bindings/python/README.md#build-a-local-wheel) 或
[WASM / npm 包](docs/BUILDING.md#wasm)的步骤。

干支计算不在本库 API 内，请用 [bazi](https://github.com/0xf3cd/bazi)。

<a id="install"></a>
## 安装与示例

先选一种语言。每节给出安装方法和一个独立示例：指定 UT1 时刻之后的下一个节气。
更多示例与完整契约见各语言的包文档。

<a id="python"></a>
### Python

需要 Python 3.11 或更新版本。在应用的虚拟环境中安装：

```sh
python -m pip install celestial-calendar
```

wheel 自带对应平台的原生库，使用时不需要编译器，也不需要另装 CelestialCalendar；导入时既不搜索系统库，也不下载备用库。
支持平台及完整契约见 [Python 包文档](bindings/python/README_ZH.md)。

下面的 Python 代码块是独立程序，可作为应用目录中的 `example.py`，用该环境的 `python example.py` 运行。

**下一个节气，还剩多少天**

以明确给定的 **2026-12-31 00:00 UT1** 为起点，不读取当前时钟。查询 2026 和 2027 的全部节气，
按实际 UT1 时刻排序，取严格晚于起点的第一个。枚举从立春开始，而不是从公历年内的第一个节气开始。

```python
import celestial_calendar as celestial

cutoff_ut1 = celestial.CivilDateTime(2026, 12, 31, 0)
cutoff_jd = celestial.ut1_to_jd(cutoff_ut1)
events = [celestial.jieqi_moment(year, jieqi) for year in (2026, 2027) for jieqi in celestial.Jieqi]
ordered = sorted(events, key=lambda event: celestial.ut1_to_jd(event.moment_ut1))
upcoming = next(event for event in ordered if celestial.ut1_to_jd(event.moment_ut1) > cutoff_jd)
remaining_days = celestial.ut1_to_jd(upcoming.moment_ut1) - cutoff_jd
print(celestial.jieqi_name(upcoming.jieqi))
print("UT1:", upcoming.moment_ut1)
print("Remaining UT1 days:", remaining_days)
```

剩余天数保留小数，是两个 UT1 儒略日之差，不是公历日期之差。此例只演示这次跨年查询，
不是覆盖整个 API 年域的通用搜索。节气查询的输入年域为 `[401, 32766]`，返回时刻的年份可能与查询年份不同；
即使输入在年域内，原生计算无法得到唯一时刻时仍会报错。

`moment_ut1` 不是 UTC 或东八区时间，显示前先确定时标转换。

今天的阴历、公历/阴历往返和完整星盘快照见 [Python 包文档](bindings/python/README_ZH.md)。

<a id="javascript"></a>
### JavaScript / TypeScript

支持 Node 22 或更新版本；浏览器端在 Chrome 和 Firefox 上测试。在应用目录中安装：

```sh
npm install @0xf3cd/celestial
```

下面的 JavaScript 代码块是独立的 ES 模块，可作为该目录中的 `example.mjs`，用 `node example.mjs` 运行。
TypeScript 声明随包提供。浏览器部署还需保留构建工具输出的 `.wasm` 地址，并以 `Content-Type: application/wasm` 提供该文件。

使用计算 API 前先调用并等待 `init()`，之后的计算 API 同步调用。并发或重复调用共享同一个初始化 promise，
结果为 `undefined`。加载失败时保留加载器原来的拒绝原因，之后再次显式调用可以重试。

**下一个节气，还剩多少天**

与 Python 例子相同，起点是明确给定的 **2026-12-31 00:00 UT1**，不是把 `Date` 时间戳当作 UT1。

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
`momentUt1` 是 UT1；示例范围、查询年域与失败条件同上。

今天的阴历、固定偏移日期往返和星盘快照见 [JavaScript 包文档](bindings/javascript/README_ZH.md)。

<a id="cpp"></a>
### C++

使用源码检出或源码归档，把头文件与应用一起编译，不需要另行构建或链接 CelestialCalendar 库。
原生发行 ZIP 不含 C++ 头文件树。

下面完整的 `quickstart.cpp` 查询冬至的 UT1 时刻：

```cpp
#include <iostream>

#include "jieqi.hpp"

int main() {
  using namespace calendar::jieqi;
  const Jieqi jq = Jieqi::冬至;
  const auto moment = jieqi_ut1_moment(2026, jq);
  std::cout << name_of(jq) << " 2026 (UT1): " << moment.year() << '-' << moment.month() << '-'
            << moment.day() << ", day fraction " << moment.fraction() << '\n';
}
```

把它存为 `build/quickstart.cpp`（`build/` 不存在就先建；Git 会忽略它），在源码根目录用支持 C++23 的 clang++ 编译运行：

```sh
# Headers include each other by basename; all three directories are needed.
clang++ -std=c++23 -I src/astro -I src/calendar -I src/util \
  build/quickstart.cpp -o build/quickstart
./build/quickstart
```

节气 API 位于 [`src/calendar/jieqi.hpp`](src/calendar/jieqi.hpp)，包括 `jieqi_ut1_moment`、`jieqi_jde`、
`JieqiGenerator`。其他功能也按领域组织在 `src/astro/`、`src/calendar/`、`src/util/` 的自包含头文件中。
月交点、黄经速率、星盘、行星照明与行星事件头文件的说明见
[API 参考的 C++ 指南（英文）](docs/API.md#cpp-guide)。其中只有星盘快照也能从 C、Python 和 JavaScript 调用。

<a id="c-abi"></a>
### C 与其他语言：C ABI

同一个查询可通过 [`celestial.h`](src/shared_lib/celestial.h) 提供的 C ABI 调用，适用于 C、ctypes 或其他 FFI。
下面是完整的 `quickstart.c`：

```c
#include <stdio.h>

#include "celestial.h"

int main(void) {
  char name[16]; /* index 21 = 冬至 in the to_index order (0 = 立春) */
  if (!get_jieqi_name(21, name, sizeof name)) {
    fprintf(stderr, "%s\n", last_error());
    return 1;
  }
  const JieqiMomentQuery m = query_jieqi_moment(2026, 21);
  if (!m.valid) {
    fprintf(stderr, "%s\n", last_error());
    return 1;
  }
  printf("%s (UT1): %d-%02u-%02u, day fraction %.6f\n", name, m.y, m.m, m.d, m.frac);
  return 0;
}
```

预编译库可从 [Releases](https://github.com/0xf3cd/celestial-calendar/releases) 选择版本后下载。
公开发行文件不需要 GitHub token；下载前请核对[原生运行环境](README.md#native-runtime)。

| 平台 | 发行文件 | 链接与运行所需文件 |
|---|---|---|
| Linux x86_64 | `linux_amd64.zip` | `lib/libcelestial_calendar.so` 及其带版本号的文件 |
| Linux arm64 | `linux_arm64.zip` | `lib/libcelestial_calendar.so` 及其带版本号的文件 |
| macOS arm64 | `macos_arm64.zip` | `lib/libcelestial_calendar.dylib` 及其带版本号的文件 |
| Windows x86_64 | `windows_x86_64.zip` | 链接用 `lib/celestial_calendar.lib`；运行用 `bin/celestial_calendar.dll` |

每个 ZIP 都包含 `include/celestial.h`。将整个 ZIP 解压到应用目录的 `native/` 下，保留所有库文件名，
因为运行时加载器也会使用带版本号的名字。Unix 上传时会解引用安装目录中的符号链接，
因此下载到的各个版本名对应的是普通文件。

Linux 上，在含有 `quickstart.c` 和 `native/` 的应用目录中编译，并记录绝对运行时搜索路径：

```sh
prefix="$PWD/native"
cc -std=c11 quickstart.c -I "$prefix/include" -L "$prefix/lib" \
  -lcelestial_calendar -Wl,-rpath,"$prefix/lib" -o quickstart_c
./quickstart_c
```

macOS 上链接 `lib/` 中的 `.dylib`，并确保加载器能找到该目录。Windows 上使用兼容 MSVC 的工具链，
链接 `lib/celestial_calendar.lib`，把 `bin/celestial_calendar.dll` 放在可执行文件旁。
使用者不要定义 `CELESTIAL_BUILDING_DLL`。

若已[自行构建共享库](docs/BUILDING.md#build)，把同一份 `quickstart.c` 存为 `build/quickstart.c`，在源码根目录中用以下 Linux 命令编译：

```sh
cc -std=c11 build/quickstart.c -I src/shared_lib -L build/shared_lib \
  -lcelestial_calendar -Wl,-rpath,"$PWD/build/shared_lib" -o build/quickstart_c
./build/quickstart_c
```

读取返回结构体前先检查 `valid`。标量与计数返回值须遵循 `celestial.h` 的各自契约，零计数可能是合法的空结果。
除 `last_error()` 外，每个导出函数都会清除或记录调用线程的错误信息；在同一线程的下一次记录调用之前，
读取或复制这个由库持有的字符串。节气时刻是 UT1，不是 UTC 或东八区日期。

原生发行包的运行环境要求（支持下限与产物实测值）以英文版的[原生运行环境表](README.md#native-runtime)为准。

<a id="features"></a>
## 功能与算法

- 公历与阴历日期互转。
- 查询节气的具体时刻。
- 日出日落、中天、曙暮光、极昼极夜；与 USNO / NOAA / JPL DE 外部参考的差异在 ±2 分钟内。
- C++ 头文件提供 Ascendant、Descendant、Midheaven、Imum Coeli，以及 Equal、Whole Sign、
  Placidus 十二宫头；地点经度东正，时刻分别传入 UT1 与 TT。
  三种宫制均拒绝地理极点和浮点舍入精度下无法确定的上升点方向；
  Placidus 还会在 `abs(latitude) >= 90° - true_obliquity` 时拒绝计算，不切换宫制。
- C++ 头文件提供日月及水星至冥王星的地心视黄道坐标、行星照明几何与逆行标记，以及太阳视赤道坐标与合朔时刻。
  水星至海王星使用 VSOP87D；`Planet::PLUTO` 使用精度较低的 Meeus 第 37 章模型，其视位置与照明几何
  JDE(TT) 域为 `[2409543.5, 2488069.5)`，居中逆行判断域为 `[2409544.0, 2488069.0)`。
- C++ 头文件以 JDE(TT) 查询平升交点、平降交点、真升交点和真降交点的真黄道、真春分点经度；
  真交点采用 Meeus 五项修正，纬度按定义恒为零，物理距离不存在。
- C++ `ephemeris.hpp` 提供日月、行星及月交点的统一瞬时黄经速率，单位为度/TT 日；
  在现有视位置模型上做局部解卷绕的七点中心差分。留站附近，速率的正负号可能与一天跨度的逆行标记不一致，
  见 [`planet.hpp`](src/astro/planet.hpp) 的 `is_retrograde`。
- 固定时刻星盘快照：以显式 UTC/UT1 民用输入、ΔT 模型和地点/宫制，一次取得十四个天体/点的位置与速率、
  四轴和十二宫头。C++ 为 [`chart.hpp`](src/astro/chart.hpp)，C 为 [`chart_snapshot_v1`](docs/API.md#c-chart-snapshot)，
  Python 为 [`chart_snapshot`](bindings/python/README_ZH.md#chart-snapshot-example)，
  JavaScript 为 [`chart.snapshot`](bindings/javascript/README_ZH.md#chart-snapshot)。
  C ABI 和两个语言包只通过这份快照提供行星、月交点、黄经速率和宫位，这些量的独立函数仅 C++ 提供。
- C++ 可按公历年查询行星的合、冲、方照、大距和精确留站，返回按 JDE(TT) 排序的事件；年份归属采用
  `[Jan 1, next Jan 1)` UTC，1972 年前沿用本库的 UT1 替代。输入年域 `[1, 32766]` 只保证计算链可用，
  不表示 VSOP87D 在远年代仍具有现代星历精度；冥王星只提供外行星事件，完整公历年域为 `[1886, 2098]`。
- 均时差与地方真太阳时。
- UT1 / UTC / TT 时标转换、闰秒与 ΔT、儒略日、恒星时、黄赤交角、章动。
- [C ABI 共享库](#c-abi)、[Python 原生 wheel](#python) 和 [JavaScript / TypeScript WASM 包](#javascript)。

<a id="choosing-inputs"></a>
### 选择输入

公历与阴历互转处理的是日期标签，不是时刻；每种算法有各自的阴历年域与日期基准。按来源与兼容用途选择，
不要把编号当作精度排名：Algo1 保留香港天文台的日期标签，Algo2 用天文方法在更宽的年域内计算，Algo3 把两者合在一起。
年域从库里读取，不要照抄：C 用 `get_supported_lunar_year_range`，Python 用 `supported_lunar_year_range()`，
JavaScript 用 `lunar.supportedYearRange()`，C++ 用各 `calendar::lunar::algoN` 的 `START_YEAR` / `END_YEAR`。
各算法的来源、年域与日期基准见 [Python](bindings/python/README.md#lunar-algorithms) 与
[JavaScript](bindings/javascript/README.md#lunar-algorithms) 包文档（英文），以及
[`algo1.hpp`](src/calendar/lunar/algo1.hpp)、[`algo2.hpp`](src/calendar/lunar/algo2.hpp)、
[`algo3.hpp`](src/calendar/lunar/algo3.hpp)。

ΔT（TT 减 UT1，单位秒）用 default，即当前项目模型 Algo5；已冻结的 Algo1 至 Algo4 只在需要与对应的早期模型比较时选用。
各模型的来源与硬年域见 [Python](bindings/python/README.md#delta-t-models) 与
[JavaScript](bindings/javascript/README.md#delta-t-models) 包文档（英文），以及 [`delta_t.hpp`](src/astro/delta_t.hpp)。

时标要分开。JD UT1 与 JDE TT 不能互换；节气结果是 UT1，而 `apparent_solar_time()` / `sun.apparentSolarTime()`
接受民用 UTC。纯 `/date` 桥接不提供 UTC 到 UT1、或 Date 到 JDE 的转换。本库不使用观测 DUT1（UT1 − UTC），
所以 UT1 结果不是 UTC 时间；本库的 UTC 模型以及两者在各年代相差多少，见[时标（英文）](docs/DESIGN.md#time-scales)。

C ABI 和两个语言包中，地理经度东正，太阳与月亮的角度单位为度。太阳距离为 AU；独立的月亮坐标接口给出千米，
星盘快照中所有天体的距离（含月亮）都是 AU。均时差是时角的度数，乘以 240 得到时间秒。

<a id="references-and-license"></a>
## 参考文献与许可证

算法来源与参考链接见英文版的 [References](README.md#references)。
[许可证范围与第三方例外](README.md#license)以英文版的 License 节为准。
