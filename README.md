# Celestial Calendar

天文计算与公历、阴历转换：查询节气时刻、日月位置、日出日落。核心是 C++23 头文件库，
也可通过 Python、JavaScript / TypeScript 或 C ABI 使用。

[English Guide](README_EN.md) · [Python](#python) · [JavaScript / TypeScript](#javascript) ·
[C++](#cpp) · [C / FFI](#c-abi) · [功能与算法](#features)

本文对应 **0.7.0 源码与接口**，不表示 0.7.0 已发布。下面的安装命令取得 PyPI / npm 当前可用的版本；
可用版本可能落后于本文，请同时核对所装版本的文档。若需要尚未发布的接口，可从完整源码构建
[本机 Python wheel](bindings/python/README.md#build-a-local-wheel) 或 [WASM / npm 包](#wasm)。

## 1. 安装与示例

先选一种语言。下面分别给出三个独立示例：今天的阴历、指定 UT1 时刻之后的下一个节气、日期往返转换。
干支计算不在本库 API 内，请用 [bazi](https://github.com/0xf3cd/bazi)。

<a id="python"></a>
### 1.1. Python

需要 Python 3.11 或更新版本。在应用的虚拟环境中安装：

```sh
python -m pip install celestial-calendar
```

wheel 自带对应平台的原生库，使用时不需要编译器；导入时既不搜索系统库，也不下载备用库。
支持平台及完整契约见 [Python 包文档](bindings/python/README.md)。
下面每个 Python 代码块都是独立程序，可作为应用目录中的 `example.py`，用该环境的 `python example.py` 运行。

**今天的阴历**

先取固定 UTC+8 下的今天，再把年月日交给阴历转换。这里选用 `ALGO3`；日期基准与年域见
[算法说明](#features)。这一步不涉及 UT1。

```python
from datetime import datetime, timedelta, timezone

import celestial_calendar as celestial

east_eight = timezone(timedelta(hours=8))
today = datetime.now(east_eight).date()
gregorian = celestial.GregorianDate.from_date(today)
lunar = celestial.gregorian_to_lunar(celestial.LunarAlgorithm.ALGO3, gregorian)
print(today, lunar)
```

`GregorianDate.from_date()` 只接受日期，不接受 `datetime`；示例先选时区、再显式取 `.date()`，
而不是让库丢弃时间或偏移。

**下一个节气，还剩多少天**

以明确给定的 **2026-12-31 00:00 UT1** 为起点，不读取当前时钟。查询 2026 和 2027 的全部节气，
按实际 UT1 时刻排序，取严格晚于起点的第一个。枚举从立春开始，不是公历年内的时间顺序。

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
即使输入在年域内，原生计算无法得到唯一时刻时仍会报错。`moment_ut1` 不是 UTC 或东八区时间，不能直接当作它们显示。

**公历与阴历往返**

```python
from datetime import date

import celestial_calendar as celestial

day = date(2024, 2, 10)
gregorian = celestial.GregorianDate.from_date(day)
algorithm = celestial.LunarAlgorithm.ALGO3
lunar = celestial.gregorian_to_lunar(algorithm, gregorian)
restored = celestial.lunar_to_gregorian(algorithm, lunar).to_date()
print(lunar)
print(restored, restored == day)
```

往返保留日期标签，不转换时区或时标。`LunarDate` 使用传统月份编号和独立的 `is_leap` 闰月标记。
公开 API 使用枚举、不可变 dataclass、普通标量与元组，不暴露 ctypes 协议。
类型错误抛 `TypeError`，Python 侧的值域检查抛 `ValueError`，原生失败抛 `CelestialError`；
合法的“没有结果”用 `None` 或空元组表示。

<a id="javascript"></a>
### 1.2. JavaScript / TypeScript

支持 Node 22 或更新版本；浏览器端在 Chrome 和 Firefox 上测试。在应用目录中安装：

```sh
npm install @0xf3cd/celestial
```

下面每个 JavaScript 代码块都是独立的 ES 模块，可作为该目录中的 `example.mjs`，用 `node example.mjs` 运行。
TypeScript 声明随包提供。计算 API 在 `await celestial.init()` 完成后同步调用；
浏览器部署还需保留构建工具输出的 `.wasm` 地址，并以 `Content-Type: application/wasm` 提供该文件。

**今天的阴历**

`/date` 子路径先把当前时间戳转换为固定 UTC+8 的民用时间。只取年月日构造公历日期，
不要把含有 `fraction` 的整个民用时间对象传给 `lunar.fromGregorian()`。

```js
import * as celestial from "@0xf3cd/celestial";
import { dateToCivilAtOffset } from "@0xf3cd/celestial/date";

await celestial.init();
const { year, month, day } = dateToCivilAtOffset(new Date(), 480);
const gregorian = { year, month, day };
const lunar = celestial.lunar.fromGregorian("algo3", gregorian);
console.log(gregorian, lunar);
```

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

**固定偏移下的日期往返**

`/date` 是纯日期桥接，不导入根入口、不加载 WASM，也不需要 `init()`。
偏移量以分钟为单位，向东为正；`480` 表示固定 UTC+8。

```js
import { civilAtOffsetToDate, dateToCivilAtOffset } from "@0xf3cd/celestial/date";

const date = new Date("2024-02-09T16:00:00.123Z");
const eastEight = dateToCivilAtOffset(date, 480);
const restored = civilAtOffsetToDate(eastEight, 480);
console.log(eastEight);
console.log(restored.toISOString(), restored.getTime() === date.getTime());
```

同一偏移下往返保留 `Date` 的毫秒时间戳。子路径还提供 `dateToCivilUtc()` 与 `civilUtcToDate()`，
但不处理 UT1 / TT 转换、闰秒、IANA 时区或夏令时。不要把 `momentUt1` 当作 UTC 传给 `civilUtcToDate()`。

根 API 读取显式字段，不隐式转换 `Date`。`GregorianDate`、`CivilDateTime`、`LunarDate` 是互斥的日期类型。
初始化前调用计算方法会抛 `CelestialError`，其 `recorded` 为 `false`；初始化后的形状或类型错误抛
`TypeError`，JavaScript 值域检查抛 `RangeError`，原生失败抛 `CelestialError`。合法的空结果用 `null` 或 `[]` 表示。
完整日期、模型与错误契约见 [JavaScript 包文档](bindings/javascript/README.md)。

<a id="cpp"></a>
### 1.3. C++：直接包含头文件

使用源码检出或源码归档，把头文件与应用一起编译，不需要另行构建或链接 CelestialCalendar 库。
原生发行 ZIP 不含 C++ 头文件树。下面的 `quickstart.cpp` 查询冬至的 UT1 时刻；编译命令在源码根目录运行。

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

```sh
# Headers include each other by basename; all three directories are needed.
clang++ -std=c++23 -I src/astro -I src/calendar -I src/util \
  quickstart.cpp -o quickstart
./quickstart
```

节气 API 位于 [`src/calendar/jieqi.hpp`](src/calendar/jieqi.hpp)，包括 `jieqi_ut1_moment`、`jieqi_jde`、
`JieqiGenerator`。其他功能也按领域组织在 `src/astro/`、`src/calendar/`、`src/util/` 的自包含头文件中。

<a id="c-abi"></a>
### 1.4. C 与其他语言：C ABI

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
公开发行文件不需要 GitHub token；下载前请核对[原生运行环境](#native-runtime)。

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

若已[自行构建共享库](#build)，可在源码根目录中用以下 Linux 命令编译同一份 `quickstart.c`：

```sh
cc -std=c11 quickstart.c -I src/shared_lib -L build/shared_lib \
  -lcelestial_calendar -Wl,-rpath,"$PWD/build/shared_lib" -o quickstart_c
./quickstart_c
```

读取返回结构体前先检查 `valid`。标量与计数返回值须遵循 `celestial.h` 的各自契约，零计数可能是合法的空结果。
除 `last_error()` 外，每个导出函数都会清除或记录调用线程的错误信息；在同一线程的下一次记录调用之前，
读取或复制这个由库持有的字符串。节气时刻是 UT1，不是 UTC 或东八区日期。

<a id="features"></a>
## 2. 功能与算法

- 公历与阴历日期互转。
- 查询节气的具体时刻。
- 日出日落、中天、曙暮光、极昼极夜；与 USNO / NOAA / JPL DE 外部参考的差异在 ±2 分钟内。
- 日月地心视黄道坐标、太阳视赤道坐标，以及合朔时刻。
- 均时差与地方真太阳时。
- UT1 / UTC / TT 时标转换、闰秒与 ΔT、儒略日、恒星时、黄赤交角、章动。
- [C ABI 共享库](#c-abi)、[Python 原生 wheel](#python) 和 [JavaScript / TypeScript WASM 包](#javascript)。

阴历转换的支持年域与日期基准取决于所选算法，不应把编号当作精度排名：

- Algo1 保留香港天文台公布的日期标签，支持阴历年 1901–2099。
- Algo2 用 VSOP87D / 截断的 ELP2000-82B 计算，支持阴历年 410–2500。TT 时刻先经过本库的 UTC 模型，
  再加固定的八小时偏移；1972 年前该模型明确以 UT1 代替，闰秒表最后一项之后保持 ΔAT 为 37 秒。
- Algo3 是预先生成的混合表，支持阴历年 1600–2199：1901–2099 使用 Algo1 / HKO，其余年份由 Algo2 生成。

公历输入输出是所选基准下的日期标签，不是时刻。年域指阴历年，并非对应公历年的 1 月 1 日到 12 月 31 日，
超出覆盖范围会被拒绝。Algo2 的 2500 年上限来自 #139 的民用日期误差预算，不是计算方法的极限。
C++ 中各 `calendar::lunar::algoN` 的 `START_YEAR` / `END_YEAR` 给出边界；
C ABI 和两个语言包均提供三种算法，可通过 `get_supported_lunar_year_range` 及其对应封装查询年域。

ΔT 的 `default` 与 Algo5 使用当前项目模型：2005 年前沿用 Algo2，其后使用截至 2026.41 的
IERS Bulletin A 拟合，再向后接锚定的长期外推。Algo1（Xu Jianwei 2008）、Algo2（Espenak and Meeus,
NASA/TP-2006-214141）、Algo3（Fred Espenak's 2014 eclipse canon）和 Algo4（IERS Bulletin A 加 USNO 预测）
均已冻结，保留用于历史比较。冻结不等于停用：Algo2 仍是 Algo3、Algo4、当前 default / Algo5 在 2005 年前的分支。

ΔT 的硬年域分别是 Algo1 的 year ≥ -4000、Algo3 的 year < 3000、Algo4 的 year < 2035；
Algo2 / Algo5 没有模型专属年界。拟合残差和测试容差不是统计意义上的误差保证。

## 3. 环境要求

使用已安装的包只需满足各自的 [Python](#python)、[JavaScript](#javascript) 或[原生运行环境](#native-runtime)要求。
从源码构建核心库才需要以下工具：

- 支持 C++23 的编译器。CI 在 Linux 和 Windows 上用 clang++ 22，在 macOS 上用 Xcode 26 的 Apple clang，
  另在 Linux 上用 g++ 14；更早的编译器可能可用，但未验证。
- CMake ≥ 3.22 和 make。
- Python 3，用于构建与测试自动化；依赖在 `Requirements.txt`。

Debian、Ubuntu 等发行版的系统 Python 受 PEP 668 保护，应使用虚拟环境，见[构建步骤](#build)。
`--setup` 在依赖已齐全时不会再安装。`Requirements.txt` 不含[静态检查工具](#lint)；
`statistics/` 的笔记本和数据采集脚本另用 `Requirements-statistics.txt`。

### 3.1. 原生发行包的运行环境

Supported 列是兼容性承诺，Measured 列只记录产物测量值，不能据此降低支持下限。
Linux 记录最大的 GLIBC / GLIBCXX 要求，macOS 记录 Mach-O deployment target，Windows 记录 Visual C++ 运行库链接方式。
CI 把同样的值写入 `build_info.json`，拒绝测量要求高于承诺的产物，并据此核对发行文件。
Linux 假定系统已提供标准的 `libstdc++.so.6` 和 `libgcc_s.so.1`；不声明 Windows 操作系统版本下限。

<a id="native-runtime"></a>
<!-- native-runtime-matrix -->
| Artifact | Supported runtime | Measured artifact property |
|---|---|---|
| `linux_amd64` | `glibc=2.28, glibcxx=3.4.21` | `glibc=2.26, glibcxx=3.4.21` |
| `linux_arm64` | `glibc=2.28, glibcxx=3.4.21` | `glibc=2.17, glibcxx=3.4.21` |
| `macos_arm64` | `macos=14.0` | `macos=14.0` |
| `windows_x86_64` | `windows=not_declared` | `msvc_runtime=static` |

<a id="build"></a>
## 4. 从源码构建

本节面向需要构建核心库的使用者与贡献者。命令均在完整源码根目录运行，产物位于 `build/`。
构建、测试与代码风格约定以 [`AGENTS.md`](AGENTS.md) 为准。Python wheel 另走
[本机 wheel 配方](bindings/python/README.md#build-a-local-wheel)，不要仅复制 `bindings/python/` 目录构建。
本机 wheel 使用宿主库，不等同于官方修复后的可移植发行 wheel，也不扩大官方支持平台。

### 4.1. macOS / Linux

已安装 C++23 编译器、CMake、make 和 Python 3 后，创建并启用虚拟环境，再运行统一入口：

```sh
python3 -m venv .venv
. .venv/bin/activate
export CXX=clang++
python project.py --all
```

`--all` 依次完成 setup、configure、build、test。也可分步运行或只执行指定任务：

```sh
python project.py --setup
python project.py --cmake --build
python project.py --test
CELESTIAL_TEST_SEED=123 python project.py --test
python project.py --bench
python project.py --help
```

随机测试默认种子为 42；`CELESTIAL_TEST_SEED` 用于重放或探索其他种子。
基准测试是可选项，不在 `--all` 内；`python project.py --clean` 清理构建产物。

### 4.2. Windows

除 Python 3 和 CMake 外，还需要 LLVM 与 make；构建使用 CMake 的 `Unix Makefiles` 生成器。
若已安装 Chocolatey，可用 `choco install -y make llvm` 安装后两者。
在源码根目录的 PowerShell 中，同时指定 LLVM 的 C / C++ 编译器，避免混用工具链：

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
$env:CXX = "clang++"
$env:CC = "clang"
python project.py --all
```

分步命令与上面相同；设置随机种子用 `$env:CELESTIAL_TEST_SEED = "123"`。

## 5. 数值验证

测试位于 `src/test/`，以外部参考数据和逐列容差检验数值。每组数据记录来源、生成方式与容差理由，
便于重建和核查；可从 `src/test/jieqi_golden_test.cpp` 看一个完整例子，具体约定见 `AGENTS.md`。
外部参考包括：

- **JPL Horizons（DE441）**：日月视位置与节气过点，由 `statistics/` 中的 `moon_horizons_crawler.py`、
  `sun_equatorial_horizons_crawler.py`、`sun_jieqi_golden_crawler.py` 采集或重放。
- **香港天文台历书**：2022–2028 年公布的节气钟表时间，整条计算链与其差异须在 60 秒内，
  主要容纳历书自身的分钟舍入。由 `automation/jieqi_table.py` 实现，入口为 `./checks.py --jieqi-table`。
- **ytliu0's ChineseCalendar**：按 commit 固定的独立阴历年表，用于验证预生成算法，
  见 `src/test/lunar/algo3_ytliu0_golden_test.cpp`。
- **ΔT 观测值**：NASA eclipse ΔT table、USNO observations、Stephenson & Morrison，
  为 UT1 ↔ TT 转换提供不依赖本库拟合模型的基准，见 `src/test/astro/julian_day_test.cpp`。
- **日出日落参考**：USNO / NOAA / JPL DE，容差为 ±2 分钟。

`statistics/` 保存数据采集脚本与评估笔记本，依赖可在虚拟环境中用
`python -m pip install -r Requirements-statistics.txt` 安装。[参考资料](#references)保留原始来源链接。

<a id="wasm"></a>
## 6. 构建 WebAssembly 与 npm 包

使用已发布 npm 包不需要本节的构建工具。本节面向源码构建与产物维护，需要 Node ≥ 22、Python 构建依赖和
emsdk 源码目录；通过 `EMSDK` 环境变量或 `build_wasm.py --emsdk` 指定 emsdk 路径。在完整源码根目录运行：

```sh
npm ci --ignore-scripts --prefix bindings/javascript
python3 toolbox/build_wasm.py
node toolbox/wasm_check.mjs
python3 toolbox/build_npm.py
```

生成模块位于 `build/wasm/celestial-jieqi.mjs` 与 `build/wasm/celestial-jieqi.wasm`，包含 `celestial.h` 的全部
29 个稳定导出。`@0xf3cd/celestial` 将其封装为 `config`、`time`、`sun`、`moon`、`jieqi`、`lunar` 命名空间，
堆指针、count/fill 协议、sret 布局与 `last_error` 留在包内部。

`build_npm.py` 使用生成模块和 `project.py` 的版本号，打出恰好12个文件的 npm 主包。
`@0xf3cd/celestial/date` 只导入纯校验函数，不导入根入口或 Emscripten glue。
同一个构建脚本还打出恰好7个文件的 npm 别名包 `celestial-calendar`，转发根入口、`/date` 的 ESM 与类型声明。
别名唯一的运行时依赖是精确同版本的 `@0xf3cd/celestial`，不含 WASM 副本。
只有两个名字解析到同一个主包安装时，才共享导出引用与初始化状态；这不是跨任意混合版本依赖图的单例保证。

The alias's MIT `LICENSE` covers its own material;
retained-component notices remain in the primary dependency.

CI 的独立 `wasm.yml` 流程构建模块与包。`celestial-wasm` 产物包含恰好10个顶层文件：
`celestial-jieqi.mjs`、`celestial-jieqi.wasm`、`LICENSE`、`THIRD_PARTY_NOTICES.txt`、
原样打包的 npm 主包、`npm-pack.json`、`npm-pack.sha256`，以及别名包、`npm-alias-pack.json`、`npm-alias-pack.sha256`。
两个元数据文件各自指定对应 tarball；发行流程先发布主包、再发布别名，不重新构建或打包。
历史 0.6.x 归档保留原来的单包格式。

同一 CI 流程核对 29 个签名与 16 个布局，重放原生生成的 389 点基准数据；
在独立于源码目录的 Node 最低支持版本与当前版本应用中安装同一对包，编译两个包名及 `/date` 的已安装类型声明，
并在 Chrome 和 Firefox 中运行 Astro / Vite 生产构建冒烟测试。

<a id="jieqi-table"></a>
## 7. 导出节气 JSON 表

只需查询节气顺序和间隔、不想链接库的应用，可用 `toolbox/jieqi_table.py` 将 `query_jieqi_moment`
导出为静态 JSON 表（#164）。先按[构建步骤](#build)生成共享库，再在源码根目录运行：

```sh
python3 toolbox/jieqi_table.py
python3 toolbox/jieqi_table.py -o jieqi.json --start-year 2000 --end-year 2031
```

第一条向标准输出写默认的 1950–2051 年表，共 24 × 102 = 2448 项。第二条指定文件与年窗，
可选年份在 `[401, 9999]` 内。输出契约如下：

- 每项为 `{year, idx, name_zh, unix_ms, iso_utc}`。`idx` 从立春 = 0 起，遵循 ABI 的 `to_index` 顺序；
  `name_zh` 来自 ABI 的 `get_jieqi_name`，便于核对映射。
- 按时刻严格递增排序。公历年内的编号顺序为 22、23、0、…、21，先小寒、大寒，不按编号排序。
- `year` 是传入查询的归属年。默认年窗中，每年的 24 个过点均落在同一公历年，检查会验证这一点；
  但高年份并不总成立，例如 9999 年的小寒落在 9998-12-31，此时 `year` 与 `iso_utc` 内的年份不同。
- 默认表多保留 2051 这一尾年，让 1950–2050 中每个时刻的后继仍在表内。
- 时标是 **UT1**，不是东八区钟表时间；同一时刻显示为 UTC+8 时，日期可能改变。
  不同时代的 UT1 / UTC 差异由表内 `timescale_note` 分别说明，不要仅凭 `iso_utc` 字段名判断时标。
- 毫秒以下截断、不四舍五入；`iso_utc` 与 `unix_ms` 表示同一毫秒。输出不含生成时间戳，
  同一 commit 的两次运行应得到相同字节。

`./checks.py --jieqi-table` 核对以上契约、HKO 历书基准，以及 `statistics/common.py` 的独立重算结果。

<a id="lint"></a>
## 8. 静态检查

C++ 使用 `clang-tidy`，Python 使用 Ruff，两者都不在 `Requirements.txt` 中。
在构建所用的虚拟环境中单独安装：

```sh
python -m pip install ruff
python -m pip install clang-tidy==22.1.8
```

CI 使用 runner 自带的 clang-tidy 22.1.2，pip 不提供该补丁版本；本地可用 22.1.x 或发行版的 `clang-tidy-22`。
检查配置在 `.clang-tidy`，启用 `WarningsAsErrors: '*'`。若 `PATH` 中的主版本不同，
用 `CLANG_TIDY` 指定所需可执行文件；它必须与仓库中的 `run-clang-tidy.py` 匹配，否则与 CI 的检查标准不同。

在源码根目录运行，以下命令也适用于 Windows：

```sh
python checks.py --ruff
python checks.py --clang-tidy
```

<a id="artifacts"></a>
## 9. 下载 CI 构建产物

本节面向贡献者与运行 CI 的维护者。普通使用者请走[公开发行下载](#releases)，无需 token。
在 GitHub 中打开所需的原生、[WASM](https://github.com/0xf3cd/celestial-calendar/actions/workflows/wasm.yml) 或
[Python wheel](https://github.com/0xf3cd/celestial-calendar/actions/workflows/python-wheel.yml) 流程，
从构建目标 commit 的已完成运行中下载产物。

自动下载可用 `toolbox/artifact_downloader.py`。在源码根目录、已安装 `Requirements.txt` 依赖的环境中，
通过环境变量 `GITHUB_TOKEN` 提供所需的 GitHub 认证，再运行：

```sh
python3 toolbox/artifact_downloader.py -s build/artifacts --unzip
python3 toolbox/artifact_downloader.py --help
```

默认下载从 HEAD 构建的完整原生、WASM、Python 产物；`-id` 可指定运行 ID，`-s` 指定目标目录，
省略 `--unzip` 则不解压。也可用 `python3 -m toolbox.artifact_downloader` 调用。

<a id="releases"></a>
## 10. 下载发行版本

使用者直接打开 [Releases](https://github.com/0xf3cd/celestial-calendar/releases)，选定版本，
下载原生 / WASM 归档、带 SHA-256 附件的 Python wheel 或源码。网页下载不需要 token。
发行版本与本文所述源码版本可能不同，应使用该发行版本随附的文档。

需要脚本批量下载的维护者可用 `toolbox/release_downloader.py`；这个可选工具使用 GitHub 认证，
并非公开下载的必要条件。在源码根目录、已安装 `Requirements.txt` 依赖且已配置 `GITHUB_TOKEN` 的环境中运行：

```sh
python3 toolbox/release_downloader.py -s build/releases
python3 toolbox/release_downloader.py --help
```

默认下载最新发行版的文件，`-s` 指定目标目录；也可用 `python3 -m toolbox.release_downloader` 调用。
CI 产物另见[上一节](#artifacts)。发布新版本的维护者须遵循 [`docs/RELEASING.md`](docs/RELEASING.md)，
下载或构建成功不等于完成发布。

<a id="todo"></a>
## 11. 后续工作

- 等工具链完整支持后再使用 modules、`std::views::enumerate` / `pairwise` 等 ranges 扩展，
  以及 moon_phase / jieqi 牛顿迭代中的 `std::generator`。
- `./checks.py --features` 通过实际编译检查特性；CI 分别检查 libstdc++ / libc++ / MSVC STL，
  当工具链已支持而代码仍保留手写替代时报告，避免清单过时。
- DUT1（UT1 − UTC）尚未建模。v0.4.0 已加入支持闰秒的 `utc_to_tt` / `tt_to_utc`，
  但不能据此把 UT1 输出直接当 UTC。闰秒实施期间两者相差不超过 0.9 秒；闰秒表冻结后的模型差异另见
  [`jieqi.hpp`](src/calendar/jieqi.hpp) 的时标说明。

<a id="references"></a>
## 12. References

* [ERFA v2.0.1](https://github.com/liberfa/erfa/tree/9915ba38c9365f8b0738269b8c2ac1fdd5f8dee3)
* [Definitions of Systems of Time](https://www.cnmoc.usff.navy.mil/Our-Commands/United-States-Naval-Observatory/Precise-Time-Department/The-USNO-Master-Clock/Definitions-of-Systems-of-Time/)
* [USNO Delta T Values](https://maia.usno.navy.mil/ser7/deltat.data)
* [SOFA Library (ANSI C)](https://www.iausofa.org/2023-10-11c)
* [Morrison, Stephenson, Hohenkerk & Zawilski, 2021 addendum to "Measurement of the Earth's rotation"](https://doi.org/10.1098/rspa.2020.0776)
* [vsop87c](https://github.com/hongzhen/vsop87c)
* [PyMeeus](https://github.com/architest/pymeeus)
* [meeus-elp82](https://www.celestialprogramming.com/meeus-elp82.html)
* [AA+ v2.55 A class framework for Computational Astronomy](http://www.naughter.com/aa.html)
* [Xu Jianwei, 寿星万年历2008版(V1.3.2)](https://web.archive.org/web/20080919020456id_/http://www.fjptsz.com/xxjs/xjw/rj/115.htm)
* [Fred Espenak, Thousand Year Canon of Solar Eclipses 1501 to 2500 (2014)](https://www.eclipsewise.com/help/deltatpoly2014.html)
* [算法系列之十八：用天文方法计算二十四节气（上）](https://github.com/leetcola/nong/wiki/算法系列之十八：用天文方法计算二十四节气（上）)
* [算法系列之十八：用天文方法计算二十四节气（下）](https://github.com/leetcola/nong/wiki/算法系列之十八：用天文方法计算二十四节气（下）)
* [算法系列之十九：用天文方法计算日月合朔（新月）](https://github.com/leetcola/nong/wiki/算法系列之十九：用天文方法计算日月合朔（新月）)
* [历书科普问题解答 - 中国科学院紫金山天文台](http://www.pmo.cas.cn/xwdt2019/kpdt2019/202203/t20220317_6399980.html)
* [农历编算法则](https://ytliu0.github.io/ChineseCalendar/rules_simp.html)
* [ytliu0 / ChineseCalendar](https://github.com/ytliu0/ChineseCalendar)
* [JPL Horizons](https://ssd.jpl.nasa.gov/horizons/)
* [Hong Kong Observatory — 24 Solar Terms](https://www.hko.gov.hk/en/gts/astronomy/Solar_Term.htm)

## 13. License

Project-authored material is licensed under the MIT License; see `LICENSE`.

The MIT grant covers the material this project authored: the C++ and C sources and headers, the
shared-library implementation and C ABI, the JavaScript and Python binding sources and type
declarations, the build, packaging, release, and validation automation, the documentation written
here, and the project-generated build and release evidence files.

The MIT grant does not extend to third-party material this project redistributes. It keeps its source
terms, with notices and attribution carried where applicable:

1. **Retained blocks inside project-maintained files** — coefficient tables, encoded datasets,
   formula transcriptions, and validation rows keep their source terms and carry a pinned in-file or
   adjacent attribution boundary.
2. **Third-party toolchain material embedded by the build** — the Emscripten, musl, and LLVM-runtime
   portions inside `celestial-jieqi.wasm` and its generated glue, and the Microsoft C/C++ runtime
   portions statically linked into the Windows DLL and the native library inside the Windows wheel.
   These are governed by their own terms and are accompanied by the canonical third-party notice
   bundle where one is required.
3. **Standalone third-party files and data** — the vendored `run-clang-tidy.py`, upstream licence
   texts under `third_party/`, frozen source captures, stored golden datasets, crawler outputs,
   notebook inputs, and book-derived anchors keep their own source attribution and terms. They reach
   the tagged tree and GitHub-generated source archives, but not the native archives, wheels, npm
   tarball, or WASM artifact as standalone files.

Retained third-party material is not relicensed by sitting beside MIT-licensed code, and this
project asserts no ownership of an upstream source's data merely because this project's code reads
or compares against it. See `THIRD_PARTY_NOTICES.txt` and the inline or adjacent attribution records
for the applicable exceptions.
