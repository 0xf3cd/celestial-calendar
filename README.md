# Celestial Calendar

Astronomical calculations and Gregorian / Chinese Lunar date conversion, with Jieqi (节气, the 24 solar terms),
Sun/Moon positions, and sunrise/sunset. The core is a C++23 header-only library, also available through Python,
JavaScript / TypeScript, and a C ABI.

[中文](README_ZH.md) · [Python](#python) · [JavaScript / TypeScript](#javascript) ·
[C++](#cpp) · [C / FFI](#c-abi) · [Features and algorithms](#features) ·
[Building](docs/BUILDING.md) · [Validation](docs/VALIDATION.md) · [Design](docs/DESIGN.md)

This guide describes version **0.9.0**. The install commands below get the latest version on PyPI or npm; an older
installed version may lack some APIs shown here, so check the documentation shipped with it. To build the current
source yourself, use the [local Python wheel recipe](bindings/python/README.md#build-a-local-wheel) or the
[WASM/npm recipe](docs/BUILDING.md#wasm).

For Ganzhi (干支) calculations, use [bazi](https://github.com/0xf3cd/bazi); this library has no Ganzhi API.

<a id="install"></a>
## Install and examples

Pick a language. Each section installs the package and runs one standalone example: the next Jieqi after a
supplied UT1 instant. The package guides carry more examples and the full contracts.

<a id="python"></a>
### Python

Python 3.11 or newer is supported. Install in your application's virtual environment:

```sh
python -m pip install celestial-calendar
```

Each wheel contains its platform's native library, so using it requires no compiler or separate CelestialCalendar
installation. Importing neither searches system libraries nor downloads a fallback. See the
[Python package guide](bindings/python/README.md) for available wheel targets and the full contract.

Each Python block is a standalone program for `example.py` in your application directory. Run it with
`python example.py` in the environment where the package is installed.

**Next Jieqi and remaining days**

The reference instant is explicitly supplied as **2026-12-31 00:00 UT1**, not taken from the current clock.
Query every Jieqi in both 2026 and 2027, sort by actual UT1 moment, and select the first strictly later event.
The enum starts at Lichun (立春), not the first event in a Gregorian year.

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

The difference between two UT1 Julian Days retains fractional days; subtracting calendar dates would lose the
time of day. This is a fixed contemporary query across one year boundary, not a search algorithm for the entire
API domain. Jieqi queries accept years in `[401, 32766]`, but an in-range calculation can still fail if it cannot
produce a unique moment. A returned UT1 year can differ from the query year.

`moment_ut1` is not UTC or a UTC+8 wall clock. Establish the time-scale conversion before displaying either.

Today's lunar date, Gregorian / lunar round trips and the complete chart snapshot are in the
[Python package guide](bindings/python/README.md).

<a id="javascript"></a>
### JavaScript / TypeScript

Node 22 or newer is supported. Browser consumers are tested on Chrome and Firefox. Install in your application
directory:

```sh
npm install @0xf3cd/celestial
```

Each JavaScript block is a standalone ES module for `example.mjs` in that directory; run it with
`node example.mjs`. TypeScript declarations ship with the package. For browser deployment, keep the emitted `.wasm`
URL available and serve the asset with `Content-Type: application/wasm`.

Call and await `init()` before using calculation methods, which then run synchronously. Concurrent and repeated
calls share one initialization promise, resolving to `undefined`. A failed load preserves the loader's original
rejection, and a later explicit call may retry.

**Next Jieqi and remaining days**

As in the Python example, the supplied cutoff is **2026-12-31 00:00 UT1**. No `Date` timestamp is treated as UT1.

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

This enumerates all terms in both years and orders them by moment, not enum value. Remaining days are fractional
UT1 days. `momentUt1` is UT1; the example scope, query-year domain and failure conditions are the same as
in the Python example.

Today's lunar date, the fixed-offset date round trip and the chart snapshot are in the
[JavaScript package guide](bindings/javascript/README.md).

<a id="cpp"></a>
### C++

Use a source checkout or source archive. The library is header-only: compile its headers with your application,
without building or linking a separate CelestialCalendar library. Native release ZIPs do not contain the C++
header tree.

This complete `quickstart.cpp` queries the UT1 moment of winter solstice, Dongzhi (冬至):

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

Save it as `build/quickstart.cpp` (create `build/` if needed; Git ignores it) and run from the source root with a
C++23-capable clang++ installation:

```sh
# Headers include each other by basename; all three directories are needed.
clang++ -std=c++23 -I src/astro -I src/calendar -I src/util \
  build/quickstart.cpp -o build/quickstart
./build/quickstart
```

The Jieqi API is in [`src/calendar/jieqi.hpp`](src/calendar/jieqi.hpp), including `jieqi_ut1_moment`, `jieqi_jde`,
and `JieqiGenerator`. Other self-contained headers are organized under `src/astro/`, `src/calendar/`, and `src/util/`.
The headers for lunar nodes, longitude rates, charts, planetary illumination and planetary events are described
in the [C++ guide of the API reference](docs/API.md#cpp-guide). Of these, the chart snapshot is also available
from C, Python and JavaScript, the interval longitude events from C and Python, and the operation capabilities
and model identities from C and Python ([`capabilities`](bindings/python/README.md#capabilities)).

[`ephemeris_events.hpp`](src/astro/ephemeris_events.hpp) adds C++ interval searches for a supplied longitude,
stationary instants and directed longitude differences; see [Longitude events](docs/EPHEMERIS_EVENTS.md)
for half-open ownership, numerical contacts and the floating-point premises.

Selected-target apparent positions at one JDE(TT) instant and house geometry at a civil
instant are available without a full snapshot, from C, Python and C++
([`positions` / `house_query`](bindings/python/README.md#independent-queries)); neither
carries the snapshot joint window, and both record the models they resolved (#355).

<a id="c-abi"></a>
### C and FFI

The C ABI in [`celestial.h`](src/shared_lib/celestial.h) supports C, ctypes, and other FFIs. This is a complete
`quickstart.c` for the same query:

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

Open a chosen version on [Releases](https://github.com/0xf3cd/celestial-calendar/releases) and download the native
ZIP for your platform. Public downloads need no GitHub token. Check the [native runtime table](#native-runtime)
before downloading.

| Platform | Release file | Files needed to link and run |
|---|---|---|
| Linux x86_64 | `linux_amd64.zip` | `lib/libcelestial_calendar.so` and its versioned files |
| Linux arm64 | `linux_arm64.zip` | `lib/libcelestial_calendar.so` and its versioned files |
| macOS arm64 | `macos_arm64.zip` | `lib/libcelestial_calendar.dylib` and its versioned files |
| Windows x86_64 | `windows_x86_64.zip` | link-time `lib/celestial_calendar.lib`; runtime `bin/celestial_calendar.dll` |

Every ZIP includes `include/celestial.h`. Extract the entire ZIP into `native/` in the application directory,
preserving all library filenames: the runtime loader also uses the versioned name. Unix uploads dereference
installed symlinks, so the downloaded versioned libraries are ordinary files.

On Linux, run from the directory containing `quickstart.c` and `native/`, recording an absolute runtime search path:

```sh
prefix="$PWD/native"
cc -std=c11 quickstart.c -I "$prefix/include" -L "$prefix/lib" \
  -lcelestial_calendar -Wl,-rpath,"$prefix/lib" -o quickstart_c
./quickstart_c
```

On macOS, link the `.dylib` from `lib/` and make that directory available to the runtime loader. On Windows, use
an MSVC-compatible toolchain, link `lib/celestial_calendar.lib`, and put `bin/celestial_calendar.dll` beside the
executable. Consumers do not define `CELESTIAL_BUILDING_DLL`.

If you [built the shared library yourself](docs/BUILDING.md#build), save `quickstart.c` as `build/quickstart.c`; the
corresponding Linux command from the source root is:

```sh
cc -std=c11 build/quickstart.c -I src/shared_lib -L build/shared_lib \
  -lcelestial_calendar -Wl,-rpath,"$PWD/build/shared_lib" -o build/quickstart_c
./build/quickstart_c
```

Check `valid` before reading a returned result structure. Follow each scalar/count contract in `celestial.h`;
a zero count can be a legitimate empty result. Every export except `last_error()` clears or records the calling
thread's error message. Read or copy the library-owned string before another recording call on the same thread.
Jieqi moments are UT1, not UTC or a UTC+8 wall date.

#### Native runtime

The Supported column is the compatibility promise; the Measured column only records properties of the built
artifact and never lowers that promise. Linux records the highest GLIBC / GLIBCXX requirement, macOS the Mach-O
deployment target, and Windows how the Visual C++ runtime is linked. CI writes the same values to
`build_info.json`, rejects an artifact whose measured requirement exceeds the promise, and checks release files
against them. Linux assumes the system provides the standard `libstdc++.so.6` and `libgcc_s.so.1`; no Windows
OS version floor is declared.

<a id="native-runtime"></a>
<!-- native-runtime-matrix -->
| Artifact | Supported runtime | Measured artifact property |
|---|---|---|
| `linux_amd64` | `glibc=2.28, glibcxx=3.4.21` | `glibc=2.26, glibcxx=3.4.21` |
| `linux_arm64` | `glibc=2.28, glibcxx=3.4.21` | `glibc=2.17, glibcxx=3.4.21` |
| `macos_arm64` | `macos=14.0` | `macos=14.0` |
| `windows_x86_64` | `windows=not_declared` | `msvc_runtime=static` |

<a id="features"></a>
## Features and algorithms

- Gregorian / lunar date conversion.
- Exact Jieqi moments.
- Sunrise, sunset, transit, twilight, polar day and polar night; differences from the USNO / NOAA / JPL DE
  external references are within ±2 minutes.
- C++ headers provide the Ascendant, Descendant, Midheaven and Imum Coeli, and Equal, Whole Sign and Placidus
  house cusps; site longitude is east-positive, and the instant is passed as both UT1 and TT. All three systems
  reject the geographic poles and an Ascendant direction unresolved at floating-point round-off; Placidus also
  rejects `abs(latitude) >= 90° - true_obliquity` and never switches system.
- C++ headers provide apparent geocentric ecliptic coordinates of the Sun, Moon and Mercury through Pluto,
  planetary illumination geometry and retrograde markers, the Sun's apparent equatorial coordinates, and new-moon
  moments. Mercury through Neptune use VSOP87D; `Planet::PLUTO` uses the lower-precision Meeus Chapter 37 model,
  whose apparent position and illumination geometry accept `[2409543.5, 2488069.5)` JDE(TT) and whose centred
  retrograde check accepts `[2409544.0, 2488069.0)`.
- C++ headers return true-ecliptic, true-equinox-of-date longitudes of the mean and true ascending and descending
  lunar nodes from JDE(TT); true nodes use the Meeus five-term correction, latitude is exactly zero by definition,
  and no physical distance exists.
- C++ `ephemeris.hpp` provides one instantaneous ecliptic longitude rate for the Sun, Moon, planets and lunar
  nodes, in degrees per TT day, by a locally unwrapped seven-point central difference over the existing apparent
  position models. Near a station its sign can differ from the one-day retrograde marker; see `is_retrograde` in
  [`planet.hpp`](src/astro/planet.hpp).
- A chart snapshot at one instant: from an explicitly scaled UTC/UT1 civil input, a ΔT model and a site/house
  system, it returns positions and rates of fourteen bodies/points, the four axes and the twelve cusps. It is
  available as C++ [`chart.hpp`](src/astro/chart.hpp), C [`chart_snapshot_v1`](docs/API.md#c-chart-snapshot),
  Python [`chart_snapshot`](bindings/python/README.md#chart-snapshot-example) and JavaScript
  [`chart.snapshot`](bindings/javascript/README.md#chart-snapshot). The C ABI and both packages still expose
  longitude rates only through this snapshot; planets, lunar nodes and houses also have
  [independent queries](bindings/python/README.md#independent-queries), which JavaScript does not expose.
- C++ yearly searches for planetary conjunction, opposition, quadrature, greatest elongation and exact stations
  return events ordered by JDE(TT). Year ownership uses `[Jan 1, next Jan 1)` UTC, with the library's UT1
  substitute before 1972. The `[1, 32766]` input year domain only guarantees that the computation runs, not that
  VSOP87D keeps modern ephemeris precision in remote eras; Pluto provides outer-planet events only, over complete
  Gregorian years `[1886, 2098]`.
- Equation of time and local apparent solar time.
- UT1 / UTC / TT conversion, leap seconds and ΔT, Julian Days, sidereal time, obliquity, nutation.
- A [C ABI shared library](#c-abi), a [native Python wheel](#python) and a
  [JavaScript / TypeScript WASM package](#javascript).

<a id="choosing-inputs"></a>
### Choosing inputs

Gregorian / lunar conversions operate on calendar-date labels, not instants, and each algorithm has its own
lunar-year domain and date basis. Choose by source and compatibility role, not by treating the algorithm number as
an accuracy ranking: Algo1 keeps Hong Kong Observatory labels, Algo2 computes dates astronomically over a wider
window, and Algo3 combines the two. Read a domain from the library rather than copying it:
`get_supported_lunar_year_range` in C, `supported_lunar_year_range()` in Python, `lunar.supportedYearRange()` in
JavaScript, and `START_YEAR` / `END_YEAR` of each `calendar::lunar::algoN` in C++. Each algorithm's source, domain
and date basis are described in the [Python](bindings/python/README.md#lunar-algorithms) and
[JavaScript](bindings/javascript/README.md#lunar-algorithms) package guides and in
[`algo1.hpp`](src/calendar/lunar/algo1.hpp), [`algo2.hpp`](src/calendar/lunar/algo2.hpp) and
[`algo3.hpp`](src/calendar/lunar/algo3.hpp).

For ΔT (TT minus UT1, in seconds), use the default, Algo5 (the current project model); select one of the frozen
Algo1 to Algo4 only to compare with that earlier model. Sources and hard year domains are listed in the
[Python](bindings/python/README.md#delta-t-models) and [JavaScript](bindings/javascript/README.md#delta-t-models)
package guides and in [`delta_t.hpp`](src/astro/delta_t.hpp).

Keep time scales separate. JD UT1 and JDE TT are not interchangeable; Jieqi results are UT1, whereas
`apparent_solar_time()` / `sun.apparentSolarTime()` take civil UTC. The pure `/date` bridge supplies no UTC-to-UT1
or Date-to-JDE conversion. The library does not use observed DUT1 (UT1 − UTC), so a UT1 result is not a UTC time;
[Time scales](docs/DESIGN.md#time-scales) describes the library's UTC model and how large the gap is in each era.

In the C ABI and both packages, geographic longitude is east-positive and solar and lunar angles are degrees.
Solar distance is AU. The standalone Moon coordinate gives kilometres, while a chart snapshot gives every distance
in AU, the Moon's included. The equation of time is degrees of hour angle, so multiply by 240 for seconds of time.

<a id="references"></a>
## References

* [ERFA v2.0.1](https://github.com/liberfa/erfa/tree/9915ba38c9365f8b0738269b8c2ac1fdd5f8dee3)
* [Definitions of Systems of Time](https://www.cnmoc.usff.navy.mil/Our-Commands/United-States-Naval-Observatory/Precise-Time-Department/The-USNO-Master-Clock/Definitions-of-Systems-of-Time/)
* [USNO Delta T Values](https://maia.usno.navy.mil/ser7/deltat.data)
* [SOFA Library (ANSI C)](https://www.iausofa.org/2023-10-11c)
* [Morrison, Stephenson, Hohenkerk & Zawilski, 2021 addendum to "Measurement of the Earth's rotation"](https://doi.org/10.1098/rspa.2020.0776)
* [vsop87c](https://github.com/hongzhen/vsop87c)
* [PyMeeus](https://github.com/architest/pymeeus)
* [Sonia Keys / meeus v3 at bfbd9ac](https://github.com/soniakeys/meeus/tree/bfbd9ac2c7f709c94f1dee190c633d24a723f628)
* [Swiss Ephemeris v2.10.3bfinal](https://github.com/aloistr/swisseph/tree/v2.10.3bfinal)
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

<a id="license"></a>
## License

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
