# CelestialCalendar Python 包

`celestial-calendar` 提供 CelestialCalendar 的 Python 绑定，用于天文计算、公历/阴历转换和节气时刻查询。
wheel 内含目标平台的原生库，运行时不需要编译器，也不必另行安装 CelestialCalendar。支持 Python 3.11 或更新版本。

[English](README.md)

本文对应 **1.0.0** 的开发接口，尚未发布；已发布的 0.x wheel 仍使用旧名称。
运行这些源码示例前，请按下文从完整 checkout 构建本地 wheel。

<a id="install"></a>
## 安装

在应用的 Python 环境中，从 PyPI 安装适合当前平台的 wheel：

```sh
python -m pip install celestial-calendar
```

以下每个 Python 代码块均可在该环境中独立作为 `example.py`，用 `python example.py` 运行；
除本包外，只用到 Python 标准库。

<a id="examples"></a>
## 示例

<a id="todays-lunar-date"></a>
### 今天的阴历日期

先取当前时刻在固定 UTC+8 下的公历日期，再用 `ALGO3` 把这个日期标签转换为阴历；这里的“今天”不取决于主机时区。
这一步不涉及 UT1，也没有将时钟 UTC 当作 UT1。

```python
from datetime import datetime, timedelta, timezone

import celestial_calendar as celestial

east_eight = timezone(timedelta(hours=8))
today = datetime.now(east_eight).date()
gregorian = celestial.GregorianDate.from_date(today)
lunar = celestial.gregorian_to_lunar(celestial.LunarAlgorithm.ALGO3, gregorian)
print(today, lunar)
```

`GregorianDate.from_date()` 只接受日期，不接受 `datetime`；示例先选固定偏移，再显式调用 `.date()`，
库不会替调用者丢弃时间或偏移。各阴历算法的日期基准与年域不同，见[阴历算法](#lunar-algorithms)。

<a id="next-jieqi"></a>
### 下一个节气，还剩多少天

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
不是覆盖整个 API 年域的通用搜索。节气查询的年域与失败边界见 [API 契约](#api-contract)。

`CivilDateTime` 在这些转换中明确表示 UT1，最后一个字段是日小数，`0.5` 表示半日；
`ut1_to_jde()` 把同一 UT1 输入转换为 TT 下的 JDE。`moment_ut1` 不是 UTC 或东八区时间，
显示为这两种时间前须先确定时标转换。

<a id="chart-snapshot-example"></a>
### 同一瞬间的完整星盘快照

`chart_snapshot` 自 0.8.0 起提供。
这里明确输入 **2026-01-01 12:00 UTC**，不读取主机时区，也不把 UTC 当作 UT1。

```python
import celestial_calendar as celestial

snapshot = celestial.chart_snapshot(
  celestial.CivilDateTime(2026, 1, 1, 0.5),
  celestial.CivilScale.UTC,
  celestial.GeoLocation(51.5, 0.0),
  celestial.HouseSystem.PLACIDUS,
)
print(snapshot.times.jd_ut1, snapshot.times.jde_tt)
for body in snapshot.bodies:
  print(body.target.name, body.longitude_deg, body.longitude_rate_deg_per_tt_day, body.distance_au)
print(snapshot.houses.cusps_deg)
```

结果是持有自身数据的冻结 dataclass，天体和宫头集合为元组。十四项顺序为日、月、水星至冥王星、
平均升/降交点、真升/降交点；每项有视地心黄经、纬度和带符号黄经速率。
黄经使用真黄道与日期分点，角度单位为度。所有天体的距离都用 AU，月球也一样；
独立的 `moon_apparent_geocentric_coordinate()` 则以千米给出月地距离，即 `MoonCoordinate.distance_km`。
黄经速率是瞬时值，单位为度/TT 日；负值表示该瞬间黄经在减小（逆行）。节点纬度是已知的零，距离为 `None`。
四轴与十二宫头均为 `[0, 360)` 度，宫头元组从第一宫开始。经度东正西负，纬度北正南负；地点只影响宫位。完整数值见[共享重放示例](../../docs/examples/chart_snapshot/README.md)。

<a id="capabilities"></a>
### 能力与模型身份

`capabilities()` 查询 POSITION、LONGITUDE_RATE、EVENTS 和 EQUATORIAL 四类 TT 目标定义域；
HOUSES、CHART 和 DELTA_T 会抛出 `ValueError`。`model_identities()` 与 `capability_text()` 提供模型与操作说明。
每个定义域边界都由该操作自身的定义常量算出，所以能力记录接受与拒绝的输入和计算本身完全一致；
NaN 边界意为「无显式界」，文字层把已验证带标注为 measured——定义域是接受边界，不是精度承诺。

```python
import celestial_calendar as celestial

records = celestial.capabilities(celestial.CapabilityOperation.EVENTS)
pluto = next(r for r in records if r.subject is celestial.Target.PLUTO)
print(pluto.domain_start_jde_tt, pluto.domain_end_jde_tt, pluto.maximum_span_tt_days)
print(celestial.capability_text(celestial.CapabilityOperation.EVENTS, celestial.Target.PLUTO))
identities = celestial.model_identities()
algo5 = next(i for i in identities if i.key == celestial.ModelKey(celestial.ModelKind.DELTA_T, 5))
print(celestial.model_identity_text(algo5.key))
```

<a id="independent-queries"></a>
### 独立位置与宫位查询

`positions()` 只计算你选择的目标在单一 JDE(TT) 的视地心位置——不评估速率、不套快照联合窗口——所以
目标子集能在完整快照拒绝被排除目标窗口的地方成功。`house_query()` 走快照的民用换算但不受联合窗口
限制，并记录它实际解析出的模型身份。两者任一错误即整体失败，不存在半份结果。

```python
import celestial_calendar as celestial

selected = celestial.positions(
  [celestial.Target.PLUTO, celestial.Target.SUN],
  2409543.5,  # Pluto's window start
)
for position in selected:
  print(position.target, position.longitude_deg, position.latitude_deg, position.distance_au, position.source)

location = celestial.GeoLocation(latitude_deg=51.5, longitude_deg=0.0)
civil = celestial.CivilDateTime(year=1885, month=1, day=1, fraction=0.0)  # before the snapshot window
houses = celestial.house_query(civil, celestial.CivilScale.UT1, location, celestial.HouseSystem.PLACIDUS)
print(houses.times, houses.houses.ascendant_deg, houses.delta_t_model, houses.nutation_model)
again = celestial.house_query(
  civil,
  celestial.CivilScale.UT1,
  location,
  celestial.HouseSystem.PLACIDUS,
  delta_t_model=houses.delta_t_model,
  nutation_model=houses.nutation_model,
)
equatorial = celestial.equatorial_positions([celestial.Target.SUN, celestial.Target.TRUE_ASCENDING], 2451545.0)
for position in equatorial:
  print(position.right_ascension_deg, position.declination_deg, position.distance_au, position.source)
```

位置、事件、能力查询和快照统一使用 `Target`，不再提供 `ChartTarget`、`EventTarget` 或 `CapabilitySubject`。
`equatorial_positions(targets, jde_tt)` 单独返回不可变的赤道坐标：赤经 `[0, 360)` 度、赤纬 `[-90, 90]` 度，
地心、当日坐标系，使用固定 IAU1980 黄经章动与真黄赤交角；物理距离为 AU，数学交点无距离。
它保持选择顺序，拒绝空选择和重复目标，不计算速率、民用时刻或宫位。

不可变 `ModelKey(kind, identity)` 标明模型种类和非零身份码。注册记录用 `.key`，位置和快照天体用 `.source`，
宫位与快照用 `.delta_t_model`、`.nutation_model`。模型键可传回相应种类的选择参数；`DEFAULT` 只用于输入，
由原生侧解析。`model_identity_text(key)` 按模型键查询文字，不按注册表位置查询。`delta_t()` 仍返回秒数标量，
但会校验原生结果的模型键。太阳距离字段改为 `distance_au`，独立月球坐标仍使用 `distance_km`。

`house_capability(delta_t_model=...)` 返回独立的 `CivilHouseCapability`，包含民用年域、允许时标、UTC 起始日期、
已解析的 Delta T 键和可选的 TT 上界，不伪造 TT 区间，也不保证几何求解成功。民用宫位政策只通过 `house_capability()` 查询。
`capability_text()` 的目标操作必须提供具体 `Target`；HOUSES、CHART 和 DELTA_T 必须使用 `subject=None`，
不能传入一个会被忽略的目标。
`FINITE_INPUT_EVALUATION` 表示没有声明硬输入区间；有限时刻仍可能计算失败，坐标有效性不证明远年代精度或物理合理性。

<a id="interval-longitude-events"></a>
### 区间黄经事件

`longitude_events()`、`longitude_stations()` 与 `longitude_difference_events()` 是 0.9.0 新增。
它们开放 [C++ 区间搜索](../../docs/EPHEMERIS_EVENTS.md)（黄经事件指南）：供定黄经的过宫与精确切触、留站、
以及带方向的两体黄经差。角度不带任何名称与解释；把精确的目标角度映射到你自己的术语即可。

```python
import celestial_calendar as celestial

crossings = celestial.longitude_events(celestial.Target.MERCURY, 0.0, 2460735.5, 2460785.5)
for event in crossings:
  print(event.jde_tt, event.longitude_deg, event.direction)

stations = celestial.longitude_stations(celestial.Target.MERCURY, 2460735.5, 2460785.5)
for station in stations:
  print(station.jde_tt, station.motion_before.value, "->", station.motion_after.value)

differences = celestial.longitude_difference_events(
  celestial.Target.MOON, celestial.Target.SUN, -60.0, 2460676.5, 2460707.5
)
```

时间为 JDE(TT)，落在公开的 2^-30 日报告格点上；速率为公开七点算子的度/TT 日。
区间 `[start, end)` 必须非空、不超过 3660 TT 日，且落在目标黄经速率域内；空元组是合法的
「无事件」结果，数值歧义或工作量超限则抛 `CelestialError`，绝不返回部分列表。
每次查询原生侧跑两遍搜索（先计数、再填充）：50 天水星窗口耗时毫秒级，文档最坏情形
（满 3660 天跨度）也远在一秒以内。

<a id="platforms-and-local-builds"></a>
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

<a id="api-contract"></a>
## API 契约

扁平的公开 API 使用不可变 dataclass、枚举、普通标量与元组，不暴露 ctypes 协议。`CivilDateTime` 有四个字段：
`year`、`month`、`day`，以及 `[0, 1)` 内的有限日小数 `fraction`，没有单独的时、分、秒字段。
时间尺度由函数名、字段名或 `CivilScale` 标明；民用时刻不会隐式转换成 Python 的 `datetime` 类型。
各函数的契约见 [`__init__.py`](src/celestial_calendar/__init__.py) 中的英文 docstring。

错误输入类型（包括错误枚举的成员）抛出 `TypeError`。未通过有限性、范围或定义域校验的值抛出 `ValueError`；
属于对应枚举类、却不是其已声明成员的对象同样抛出 `ValueError`。
原生无效输入同样抛出 `ValueError`；计算、资源或内部失败以及损坏的成功载荷抛出 `CelestialError`。
其 `operation` 属性为公开函数名；`recorded` 表示原生侧报告了非零错误种类，即使消息分配失败而为空也为真。
合法的“无结果”仍以 `None` 或 `()` 表示。日志设置函数与 `LogVerbosity` 已移除，错误不会自动打印。

`local_apparent_sidereal_time(jd_ut1, longitude_deg)` 接受有限的 UT1 下 JD，其公历年份须在 `[401, 32766]` 内；
地理经度须有限、东正西负，范围为 `[-180, 180]` 度。结果为 `[0, 360)` 度。
年份范围由原生边界校验，超出年域的 JD 抛出 `ValueError`。
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
计数/填充及按字节定长的文字协议另有单缓冲区 64 MiB 的绑定层资源上限，超限会在分配前抛出 `CelestialError`。

<a id="chart-inputs-and-failure-boundaries"></a>
### 星盘输入与失败边界

`chart_snapshot(civil_dt, scale, location, system, model=DeltaTModel.DEFAULT)` 接受 `CivilDateTime`、
`CivilScale.UTC` / `UT1`、`GeoLocation`，以及 `HouseSystem.EQUAL` / `WHOLE_SIGN` / `PLACIDUS`。
`DEFAULT` 由原生侧解析，目前为 `ALGO5`；其他模型用 `DeltaTModel` 或相应种类的 `ModelKey` 显式选择。它不接受标准库 `datetime`，
本地钟表时间须由调用者先转换成声明的输入时标，不能丢弃时区后直接传入。

Python 在原生计算前检查类型、有限性和 ABI 整数宽度，日期、年份和地理定义域由原生侧检查。类型错误抛 `TypeError`，包括给 `scale`、`system` 或 `model`
传普通字符串或其他枚举的成员，以及在数值字段里放枚举成员。属于对应枚举类、却不是其已声明成员的对象，
与无效公历日期、非有限数和原始值越界一样抛 `ValueError`。原始值的范围是：公历年 `[1885, 2100)`，
日小数 `[0, 1)`，UTC 从 1972-01-01 开始（更早的输入请选 `CivilScale.UT1`），纬度 `(-90, 90)`，
经度 `[-180, 180]`。原生核心负责转换后 TT 的共同域 `[2409543.875, 2488069.125)` JDE、
Algo4 的额外限制 JDE `< 2464328.5`、所有宫制在浮点舍入精度下无法确定的上升点方向，
以及 Placidus 随日期变化的极区限制。原生定义域和几何拒绝抛 `ValueError`；计算失败抛 `CelestialError`。
不返回部分结果，也不替换宫制。民用宫位查询共用转换后 TT 的 Algo4 严格上界，2034 年末部分 UT1 输入因此被拒绝。
原始民用年份合法不保证转换后的完整快照可计算。

输入为 UTC 时，结果里的 UT1 是模型值，不是观测 DUT1；UTC 如何换算到 TT 与 UT1，见
[Time scales](../../docs/DESIGN.md#time-scales)。
插入的闰秒不在民用日小数输入内。Python 结果复制自 C 协议，不保留原生缓冲区；
不兼容的字段或天体集合扩展需要显式新形状或新主版本。

<a id="date-bridges-and-lunar-round-trip"></a>
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

<a id="lunar-algorithms"></a>
### 阴历算法

公历输入与输出都是所选算法日期基准下的日期标签，不是瞬间。各算法的来源、阴历年域、选用场景与日期基准见英文版的
[Lunar algorithms](README.md#lunar-algorithms)。

`supported_lunar_year_range(algorithm)` 查询原生算法包含两端的**阴历年**范围，返回 `LunarYearRange(start, end)`。
公历日期覆盖范围从阴历 `start` 年首日到阴历 `end` 年末日，而非这些公历年份的一月至十二月。
`lunar_year_info(algorithm, year)` 返回公历 `first_day`、传统 `leap_month` 月号或 `None`，
以及按历法顺序排列的 `month_lengths`，闰月紧随同名普通月份。
`lunar_to_gregorian()` 接受 `LunarDate`，使用 `[1, 12]` 内的传统月号和显式 `is_leap` 布尔值，
而非月份的位置索引。

`lunar_year_info()` 和 `lunar_to_gregorian()` 直接由原生计算检查日期与阴历年范围。
Python 包不另存范围表或缓存，不在导入或计算前查询范围。

有效公历标签超出所选算法覆盖范围、阴历年越界，或阴历闰月/日组合不存在，均抛出 `ValueError`。

<a id="delta-t-models"></a>
### Delta T 模型

`delta_t(year, model)` 对有限的十进制公历年返回 TT − UT1 的秒数。各模型的来源、硬年域与选用场景见英文版的
[Delta T models](README.md#delta-t-models)。

<a id="license"></a>
## 许可证

[许可证范围与第三方例外](README.md#license)以英文版的 License 节为准。
