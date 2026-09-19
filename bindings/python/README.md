# CelestialCalendar Python 包

`celestial-calendar` 提供 CelestialCalendar 的 Python 绑定，用于天文计算、公历/阴历转换和节气时刻查询。
wheel 内含目标平台的原生库，运行时不需要编译器，也不必另行安装 CelestialCalendar。支持 Python 3.11 或更新版本。

[English guide](https://github.com/0xf3cd/celestial-calendar/blob/main/README_EN.md)

本文对应当前 `0.7.0` 源码。PyPI 上的版本可能滞后；以下安装命令获取已发布版本，不表示 `0.7.0` 已发布。
使用示例前请核对安装版本是否提供相应 API；需要当前源码时，可按下文从完整 checkout 构建本地 wheel。

## 安装

在应用的 Python 环境中，从 PyPI 安装适合当前平台的 wheel：

```sh
python -m pip install celestial-calendar
```

以下每个 Python 代码块均可在该环境中独立作为 `example.py`，用 `python example.py` 运行；
除本包外，只用到 Python 标准库。

## 示例

### 今天的阴历日期

先取当前时刻在固定 UTC+8 下的公历日期，再转换为阴历；这里的“今天”不取决于主机时区。

```python
from datetime import datetime, timedelta, timezone

import celestial_calendar as celestial

east_eight = timezone(timedelta(hours=8))
today = datetime.now(east_eight).date()
gregorian = celestial.GregorianDate.from_date(today)
lunar = celestial.gregorian_to_lunar(celestial.LunarAlgorithm.ALGO3, gregorian)
print("UTC+8 公历日期：", gregorian.to_date())
print("阴历日期：", lunar)
```

先选固定偏移，再显式调用 `.date()`。`GregorianDate.from_date()` 不接受 `datetime`，不会替调用者丢弃时间或偏移。
这里没有将时钟 UTC 当作 UT1；阴历日期基准与算法年域见下文。

### 指定 UT1 时刻与节气查询

`CivilDateTime` 在下面的转换中明确表示 UT1，`0.5` 表示半日；转换结果 `jde` 使用 TT。
冬至结果的 `moment_ut1` 仍是 UT1，不是 UTC+8 显示时间。

```python
import celestial_calendar as celestial

ut1 = celestial.CivilDateTime(2026, 8, 16, 0.5)
jde = celestial.ut1_to_jde(ut1)
winter_solstice = celestial.jieqi_moment(2026, celestial.Jieqi.DONGZHI)

print(jde)
print(winter_solstice.moment_ut1)
```

## 平台与本地构建

正式发布使用下列四种平台的 wheel；PyPI 与对应
[GitHub Release](https://github.com/0xf3cd/celestial-calendar/releases) 使用逐字节相同的四个 wheel：

| 系统 | 架构 | wheel 文件名中必需的标签 |
|---|---|---|
| Linux (manylinux 2.28) | x86_64 | `manylinux_2_28_x86_64` |
| Linux (manylinux 2.28) | aarch64 | `manylinux_2_28_aarch64` |
| macOS 14 或更新版本 | arm64 | `macosx_14_0_arm64` |
| Windows | AMD64 | `win_amd64` |

Linux wheel 文件名可能包含其他兼容的 manylinux 标签。每个都是面向相应平台上 Python 3.11 或更新版本的 `py3` wheel。
不发布其他平台的 wheel，例如 Intel macOS、Windows on ARM 或基于 musl 的 Linux。

<a id="build-a-local-wheel"></a>

### 构建本地 wheel

使用完整源码 checkout，不能只复制 `bindings/python`。下面的 Linux shell 示例需要 Python 3.11 或更新版本、
系统 CMake 3.22 或更新版本，以及已安装的 C/C++23 工具链。示例选用 `clang-22` 和 `clang++-22`；
若本机编译器名称不同，请相应设置 `CC` 和 `CXX`。现有的哈希锁定依赖提供构建前端、后端和 Ninja。
从仓库根目录运行，使用尚未占用的构建、虚拟环境和输出目录：

```sh
python3 -m venv build/local-wheel-env
. build/local-wheel-env/bin/activate
python -m pip install --require-hashes -r bindings/python/requirements-build.txt
CC=clang-22 CXX=clang++-22 python -m build \
  --wheel --no-isolation \
  --outdir build/local-wheelhouse \
  --config-setting=build-dir="$PWD/build/local-wheel-build" \
  bindings/python
deactivate
```

将 `build/local-wheelhouse` 中生成的 `.whl` 安装到应用环境。这只构建 wheel，不构建 sdist。
该 wheel 使用本机原生库，不是经过修复的官方 wheel，也不是可移植的发行 wheel；它不扩展上表的支持平台。

## API 契约

扁平的公开 API 使用不可变 dataclass 与枚举。`CivilDateTime` 有四个字段：`year`、`month`、`day`，
以及 `[0, 1)` 内的有限日小数 `fraction`，没有单独的时、分、秒字段。
时间尺度由函数名或字段名标明；民用时刻不会隐式转换成 Python 的 `datetime` 类型。

错误输入类型（包括错误枚举的成员）抛出 `TypeError`。未通过有限性、范围或定义域校验的值抛出 `ValueError`。
原生边界报告的失败抛出 `CelestialError`，其 `operation` 属性为公开函数名，`recorded` 属性表明消息是否来自
原生错误通道。合法的“无结果”仍以 `None` 或 `()` 表示。

`local_apparent_sidereal_time(jd_ut1, longitude_deg)` 接受有限的 UT1 下 JD，其公历年份须在 `[401, 32766]` 内；
地理经度须有限、东正西负，范围为 `[-180, 180]` 度。结果为 `[0, 360)` 度。
年份范围由原生边界校验，因此超出年域的 JD 抛出 `CelestialError`，而非 Python 有限性与经度校验使用的 `ValueError`。
`apparent_solar_time()` 同样接受 `[-180, 180]` 度范围内、东正西负的地理经度，但其民用时刻输入是 UTC。

`jieqi_moment(year, jieqi)` 返回 `JieqiMoment(jieqi, moment_ut1)`。其中的 `CivilDateTime` 是 UT1，
不是 UTC 或东八区钟表时间；同一瞬间显示为 UTC+8 时，日期可能改变。显示为 UTC 或固定偏移时间前，须先确定时标转换。
返回的 UT1 年份可能与请求年份不同。输入年域为 `[401, 32766]`，但即使输入在范围内，
原生计算无法得到唯一时刻时仍会抛出 `CelestialError`。

`moon_phase_moments(year, phase)`、`sun_longitude_crossings(year, longitude_deg)` 和 `new_moons_in_year(year)`
接受 `[1, 32766]` 内的公历年份。

`sun_longitude_crossings()` 返回太阳到达指定视地心黄经时的 TT 下 JDE 元组。
`longitude_deg` 须有限，且在 `[0, 360)` 度内。原生失败使用
`CelestialError.operation == "sun_longitude_crossings"`。

`new_moons_after(jde, count)` 接受 `[0, 4096]` 内的 `count`，零返回 `()`。
上限使单个原生输出缓冲区不超过 32 KiB。

### 纯日期桥接与阴历往返

`GregorianDate(year, month, day)` 是前推公历日期，与 `CivilDateTime`、`LunarDate` 分开。
计算调用会校验公历日期和民用时刻输入的年份是否在 `[1, 32767]` 内，以及月、日是否合法。
本项目日期类型的范围仍宽于标准库日期类型，构造函数不以 9999 年为上限。

```python
from datetime import date

import celestial_calendar as celestial

day = date(2026, 8, 15)
gregorian = celestial.GregorianDate.from_date(day)
print(gregorian.to_date())

algorithm = celestial.LunarAlgorithm.ALGO3
lunar = celestial.gregorian_to_lunar(algorithm, day)
print(celestial.lunar_to_gregorian(algorithm, lunar).to_date())
```

`GregorianDate.from_date(value)` 是静态工厂：即使通过 `GregorianDate` 子类调用，也总是返回确切的 `GregorianDate`
类型。它接受标准库年域 `[1, 9999]` 内的 `datetime.date` 实例及其子类，但拒绝所有 `datetime.datetime` 实例及其子类，
无论是否带时区，均抛出 `TypeError`。
`gregorian_to_lunar(algorithm, date)` 接受 `GregorianDate`，也接受上述标准库纯日期输入；
不接受 `datetime`，也不会丢弃其时间或偏移。

`GregorianDate.to_date()` 校验字段后返回标准库 `datetime.date`。字段类型错误抛出 `TypeError`；
年份超出 `[1, 9999]` 或公历月/日无效时抛出 `ValueError`。
两个桥接方法都保留年/月/日标签。用于阴历转换时，这个标签属于下文所选算法的日期基准；
两者都不进行时区或 UT1/UTC/TT 转换。

### 阴历算法

Gregorian inputs and outputs are calendar-date labels on the selected algorithm's basis, not instants.
Choose the source and compatibility role you need, not an assumed accuracy ranking:

| Algorithm | Source and status | Lunar-year domain | Selection role |
|---|---|---|---|
| `LunarAlgorithm.ALGO1` | Published Hong Kong Observatory date labels, stored as a table | `[1901, 2099]` | HKO almanac compatibility |
| `LunarAlgorithm.ALGO2` | Computed from VSOP87D and truncated ELP2000-82B | `[410, 2500]` | Astronomical construction over the wider computed window |
| `LunarAlgorithm.ALGO3` | Baked hybrid: algo1/HKO in 1901-2099, algo2-generated dates elsewhere | `[1600, 2199]` | Table lookup with HKO compatibility in the overlapping years |

Algo2 assigns dates by rendering TT moments through the library's UTC model and then applying a fixed eight-hour
eastward offset. Before 1972, that model deliberately uses UT1 as a proxy because historical UTC is not modelled.
From 1972 it uses the leap-second table; after the final entry (2017) it holds Delta AT (TAI minus UTC) at 37 seconds.
Algo1 preserves HKO's published labels rather than imposing this rule on their history; algo3 inherits the basis of
each source slice. The 2500 ceiling is the computed algorithm's enforced civil-date domain, not a physical limit of
VSOP87D or ELP2000-82B.

`supported_lunar_year_range(algorithm)` 查询原生算法包含两端的**阴历年**范围，返回 `LunarYearRange(start, end)`。
公历日期覆盖范围从阴历 `start` 年首日到阴历 `end` 年末日，而非这些公历年份的一月至十二月。
`lunar_year_info(algorithm, year)` 返回公历 `first_day`、传统 `leap_month` 月号或 `None`，
以及按历法顺序排列的 `month_lengths`，闰月紧随同名普通月份。
`lunar_to_gregorian()` 接受 `LunarDate`，使用 `[1, 12]` 内的传统月号和显式 `is_leap` 布尔值，
而非月份的位置索引。

`lunar_year_info()` 和 `lunar_to_gregorian()` 每次调用都先查询一次原生范围，再校验阴历年。
Python 包不另存范围表或缓存，也不在导入时查询范围。阴历年越界抛出 `ValueError`；原生范围查询失败抛出 `CelestialError`，
其公开操作名为调用者 `"lunar_year_info"` 或 `"lunar_to_gregorian"`，而非内部查询。

有效公历标签超出所选算法覆盖范围，或阴历闰月/日组合不存在，均抛出 `CelestialError`。
例如，`gregorian_to_lunar(LunarAlgorithm.ALGO1, GregorianDate(1900, 1, 1))` 能通过公历校验，
但抛出 `CelestialError`；`lunar_year_info(LunarAlgorithm.ALGO1, 1900)` 则抛出 `ValueError`。

### Delta T 模型

`delta_t(year, model)` returns TT minus UT1 in seconds for a finite decimal Gregorian year.
The default is the current project model; the named older models allow historical comparison:

| Model | Source and status | Hard year domain | Selection role |
|---|---|---|---|
| `DeltaTModel.DEFAULT`, `DeltaTModel.ALGO5` | Current project model: algo2 before 2005, IERS Bulletin A fit through about 2026.41, then anchored Morrison et al. (2021) long-term extrapolation | No model-specific bound | Current project calculations |
| `DeltaTModel.ALGO1` | Xu Jianwei's 2008 model, frozen | `year >= -4000` | Comparison with that historical model |
| `DeltaTModel.ALGO2` | Espenak and Meeus, NASA/TP-2006-214141, frozen | No model-specific bound | NASA polynomial comparison; live pre-2005 branch of algo3, algo4, and algo5 |
| `DeltaTModel.ALGO3` | Fred Espenak's 2014 eclipse canon, frozen | `year < 3000` | Comparison with that later polynomial model |
| `DeltaTModel.ALGO4` | IERS Bulletin A observations and USNO predictions, frozen | `year < 2035` | Comparison with the previous project fit/prediction model |

"Frozen" describes model maintenance, not dead code: algo2 still supplies the pre-2005 values used by the current
default. A hard domain is an input contract, not an accuracy claim. Fitted residuals and test tolerances are not
statistical error guarantees, and an unbounded model does not promise useful accuracy at arbitrary years.

## License

Project-authored package material is licensed under MIT. Third-party components retain their own terms
in the included `THIRD_PARTY_NOTICES.txt`; each section states where it applies. Source and issue
tracking are at <https://github.com/0xf3cd/celestial-calendar>.
