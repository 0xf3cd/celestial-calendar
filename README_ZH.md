# Celestial Calendar

天文计算与公历、阴历转换：查询节气时刻、日月位置、日出日落。核心是 C++23 头文件库，
也可通过 Python、JavaScript / TypeScript 或 C ABI 使用。

[English](README.md) · [Python](#python) · [JavaScript / TypeScript](#javascript) ·
[C++](#cpp) · [C / FFI](#c-abi) · [功能与算法](#features) ·
[构建（英文）](docs/BUILDING.md) · [验证（英文）](docs/VALIDATION.md) · [设计（英文）](docs/DESIGN.md)

本文描述当前源码，含尚未发布的改动。从 0.x 升级请先读[迁移指南（英文）](docs/MIGRATING_1_0.md)。
下面的安装命令取得 PyPI / npm 上的最新已发布版本，请以所装版本附带的文档为准。
要自行构建当前源码，可按[本机 Python wheel](bindings/python/README.md#build-a-local-wheel) 或
[WASM / npm 包](docs/BUILDING.md#wasm)的步骤。

干支计算不在本库 API 内，请用 [bazi](https://github.com/0xf3cd/bazi)。

<a id="install"></a>
## 安装与示例

先选一种语言。每节给出安装方法和一个独立示例：查询 2026 年冬至。
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

查询冬至，并打印其 UT1 日期：

```python
import celestial_calendar as celestial

event = celestial.jieqi_moment(2026, celestial.Jieqi.DONGZHI)
moment = event.moment_ut1
print(f"{celestial.jieqi_name(event.jieqi)} (UT1): {moment.year:04d}-{moment.month:02d}-{moment.day:02d}")
```

预期输出：

```text
冬至 (UT1): 2026-12-21
```

`moment_ut1` 是 UT1，不是 UTC 或东八区时间。显示本地时间请按
[UTC 输入 → 节气 → 东八区指南（英文）](docs/UTC_CONVERSIONS.md#next-jieqi-from-a-utc-input-displayed-at-utc8)转换。
节气查询的输入年域为 `[401, 32766]`，返回时刻的年份可能与查询年份不同；
即使输入在年域内，原生计算无法得到唯一时刻时仍会报错。

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

同一个冬至查询，预期输出与上面相同：

```js
import * as celestial from "@0xf3cd/celestial";

await celestial.init();
const event = celestial.jieqi.moment(2026, celestial.Jieqi.DONGZHI);
const { year, month, day } = event.momentUt1;
const date = [year, month, day].map((value, index) => String(value).padStart(index === 0 ? 4 : 2, "0")).join("-");
console.log(`${celestial.jieqi.name(event.jieqi)} (UT1): ${date}`);
```

`momentUt1` 是 UT1，不是 UTC；查询年域与失败条件同 Python。
显示为东八区时间见 [UTC/节气指南（英文）](docs/UTC_CONVERSIONS.md#next-jieqi-from-a-utc-input-displayed-at-utc8)。

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
[API 参考的 C++ 指南（英文）](docs/API.md#cpp-guide)。星盘快照也能从 C、Python 和 JavaScript 调用；
独立黄道／赤道位置、民用时刻宫位、区间事件和能力查询另有 C 与 Python 接口，JavaScript 不暴露这些查询。

<a id="c-abi"></a>
### C 与其他语言：C ABI

同一个查询可通过 [`celestial.h`](src/shared_lib/celestial.h) 提供的 C ABI 调用，适用于 C、ctypes 或其他 FFI。
下面是完整的 `quickstart.c`：

```c
#include <stdio.h>
#include <stdlib.h>

#include "celestial.h"

int main(void) {
  const uint8_t index = 21; /* 冬至; index 0 is 立春. */
  const uint32_t size = celestial_get_jieqi_name(index, NULL, 0);
  if (celestial_last_error_kind() != CELESTIAL_ERROR_NONE || size == 0) {
    fprintf(stderr, "%s\n", celestial_last_error());
    return 1;
  }
  char *name = malloc(size);
  if (name == NULL) return 1;
  const uint32_t required = celestial_get_jieqi_name(index, name, size);
  if (celestial_last_error_kind() != CELESTIAL_ERROR_NONE || required != size) {
    fprintf(stderr, "%s\n", celestial_last_error());
    free(name);
    return 1;
  }
  const CelestialJieqiMomentQuery m = celestial_query_jieqi_moment(2026, index);
  if (celestial_last_error_kind() != CELESTIAL_ERROR_NONE || !m.valid) {
    fprintf(stderr, "%s\n", celestial_last_error());
    free(name);
    return 1;
  }
  printf("%s (UT1): %d-%02u-%02u, day fraction %.6f\n", name, m.y, m.m, m.d, m.frac);
  free(name);
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

读取返回结构体前先检查 `valid`。零计数可能是合法的空结果；判断失败以 `celestial_last_error_kind()` 为准，
即使消息为空也一样。它和 `celestial_last_error()` 均保留错误状态，其他导出函数会清除或记录调用线程的错误种类与消息。
在同一线程的下一次记录调用之前，读取或复制由库持有的消息字符串。字符串长度计入结尾 NUL；`NULL, 0` 查询所需容量，
正容量会截断并补 NUL。节气时刻固定通过 Algo5 转为 UT1，不是 UTC 或东八区日期。ABI 演进规则见
[C API 指南（英文）](docs/API.md#c-chart-snapshot)。

从 1.1.0 起，预编译的 macOS arm64 库要求 macOS 15 或更新版本。
完整运行环境要求（支持下限与产物实测值）以英文版的[原生运行环境表](README.md#native-runtime)为准。

<a id="features"></a>
## 功能与算法

- 公历与阴历日期互转。
- 查询节气的具体时刻，支持[导出离线 JSON 表](docs/VALIDATION.md#jieqi-table)。
- 日出日落、中天、曙暮光、极昼极夜；与 USNO / NOAA / JPL DE 外部参考的差异在 ±2 分钟内。
- C++ 头文件提供 Ascendant、Descendant、Midheaven、Imum Coeli，以及 Equal、Whole Sign、
  Placidus 十二宫头；地点经度东正，时刻分别传入 UT1 与 TT。
  三种宫制均拒绝地理极点和浮点舍入精度下无法确定的上升点方向；
  Placidus 还会在 `abs(latitude) >= 90° - true_obliquity` 时拒绝计算，不切换宫制。
- C++ 头文件提供日月、水星至冥王星及月交点的地心视黄道／赤道位置，以及行星照明几何、逆行标记与合朔时刻。
  水星至海王星使用 VSOP87D；`Planet::PLUTO` 使用精度较低的 Meeus 第 37 章模型，其视位置与照明几何
  JDE(TT) 域为 `[2409543.5, 2488069.5)`，居中逆行判断域为 `[2409544.0, 2488069.0)`。
- C++ 头文件以 JDE(TT) 查询平升交点、平降交点、真升交点和真降交点的真黄道、真春分点经度；
  真交点采用 Meeus 五项修正，纬度按定义恒为零，物理距离不存在。
- C++ `ephemeris.hpp` 提供日月、行星及月交点的统一瞬时黄经速率，单位为度/TT 日；
  在现有视位置模型上做局部解卷绕的七点中心差分。留站附近，速率的正负号可能与一天跨度的逆行标记不一致，
  见 [`planet.hpp`](src/astro/planet.hpp) 的 `is_retrograde`。
- 固定时刻星盘快照：以显式 UTC/UT1 民用输入、ΔT 模型和地点/宫制，一次取得十四个天体/点的位置与速率、
  四轴和十二宫头。C++ 为 [`chart.hpp`](src/astro/chart.hpp)，C 为 [`celestial_chart_snapshot`](docs/API.md#c-chart-snapshot)，
  Python 为 [`chart_snapshot`](bindings/python/README_ZH.md#chart-snapshot-example)，
  JavaScript 为 [`chart.snapshot`](bindings/javascript/README_ZH.md#chart-snapshot)。
  C ABI 和两个语言包仍只通过这份快照提供单时刻黄经速率查询；行星、月交点也有
  [独立位置与宫位查询](bindings/python/README_ZH.md#independent-queries)，
  JavaScript 则不暴露这两个独立查询。
- 区间黄经事件（供定黄经过宫、留站、带方向的两体黄经差）：C++ 见
  [`ephemeris_events.hpp`](src/astro/ephemeris_events.hpp) 与
  [黄经事件指南](docs/EPHEMERIS_EVENTS.md)；同一查询在 C ABI 为
  `celestial_longitude_events`／`celestial_longitude_stations`／`celestial_longitude_difference_events`，在 Python 为
  [`longitude_events` 等](bindings/python/README_ZH.md#interval-longitude-events)。JavaScript 不暴露事件 API。
- 操作能力与模型身份（每个公开操作接受什么输入、结果背后站着哪些模型）：C ABI 为
  `celestial_capabilities`／`celestial_house_capability`／`celestial_model_identities` 及操作／模型说明文本，Python 为
  [`capabilities` 等](bindings/python/README_ZH.md#capabilities)，C++ 为
  [`capability.hpp`](src/astro/capability.hpp)。JavaScript 不暴露能力 API。
- 独立位置与宫位查询：按调用方选择的非空目标子集在单一 JDE(TT) 取视地心黄道或赤道位置（不算
  速率、不套快照联合窗口），以及民用时刻的宫位几何；C ABI 为
  `celestial_positions`／`celestial_equatorial_positions`／`celestial_house_query`，Python 为
  [`positions`／`equatorial_positions`／`house_query`](bindings/python/README_ZH.md#independent-queries)，C++ 为
  [`ephemeris.hpp`](src/astro/ephemeris.hpp) 的 `position`／`equatorial_position` 与
  [`house.hpp`](src/astro/house.hpp) 的数值宫位操作。C 与 Python 结果带解析后的模型／来源身份；民用宫位查询
  遵循共用民用策略，但不受快照的联合目标／速率窗口限制。JavaScript 不暴露这些独立查询。
- C++ 可按公历年查询行星的合、冲、方照、大距和精确留站，返回按 JDE(TT) 排序的事件；年份归属采用
  `[Jan 1, next Jan 1)` UTC，1972 年前沿用本库的 UT1 替代。输入年域 `[1, 32766]` 不保证计算总能成功，
  不表示 VSOP87D 在远年代仍具有现代星历精度；冥王星只提供外行星事件，完整公历年域为 `[1886, 2098]`。
- 均时差与地方真太阳时。
- UT1 / UTC / TT 时标转换、闰秒与 ΔT、儒略日、恒星时、黄赤交角、章动。
- [C ABI 共享库](#c-abi)、[Python 原生 wheel](#python) 和 [JavaScript / TypeScript WASM 包](#javascript)。

<a id="choosing-inputs"></a>
### 选择输入

公历与阴历互转处理的是日期标签，不是时刻；每种算法有各自的阴历年域与日期基准。按来源与兼容用途选择，
不要把编号当作精度排名：Algo1 保留香港天文台的日期标签，Algo2 用天文方法在更宽的年域内计算，Algo3 把两者合在一起。
年域从库里读取，不要照抄：C 用 `celestial_get_supported_lunar_year_range`，Python 用 `supported_lunar_year_range()`，
JavaScript 用 `lunar.supportedYearRange()`，C++ 用各 `calendar::lunar::algoN` 的 `START_YEAR` / `END_YEAR`。
各算法的来源、年域与日期基准见[阴历算法（英文）](docs/ALGORITHMS.md#lunar-algorithms)。

ΔT（TT 减 UT1，单位秒）用 default，即当前项目模型 Algo5；已冻结的 Algo1 至 Algo4 只在需要与对应的早期模型比较时选用。
各模型的来源与硬年域见 [Delta T 模型（英文）](docs/ALGORITHMS.md#delta-t-models)。

时标要分开。JD UT1 与 JDE TT 不能互换；节气结果是 UT1，而 C 的 `celestial_apparent_solar_time()`、
Python 的 `apparent_solar_time()` 和 JavaScript 的 `sun.apparentSolarTime()`
接受民用 UTC。纯 `/date` 桥接不提供 UTC 到 UT1、或 Date 到 JDE 的转换。本库不使用观测 DUT1（UT1 − UTC），
所以 UT1 结果不是 UTC 时间；本库的 UTC 模型以及两者在各年代相差多少，见[时标（英文）](docs/DESIGN.md#time-scales)。

C ABI 和两个语言包中，地理经度东正，太阳与月亮的角度单位为度。太阳距离为 AU；独立的月亮坐标接口给出千米，
星盘快照中所有天体的距离（含月亮）都是 AU。均时差是时角的度数，乘以 240 得到时间秒。

<a id="references-and-license"></a>
## 参考文献与许可证

算法来源与参考链接见英文版的 [References](README.md#references)。
[许可证范围与第三方例外](README.md#license)以英文版的 License 节为准。
