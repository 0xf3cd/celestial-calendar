# Celestial Calendar

> A C++23-style library that performs astronomical calculations and date conversions among various calendars, including Gregorian, Lunar, and Chinese Ganzhi calendars.

Five ways in, depending on what you are here for:

* **C++ users** — the library is header-only; start at §1.1, then browse §2 Features.
* **Python users** — install a platform wheel and `import celestial_calendar`; §1.3 shows the package entry point.
* **JavaScript / TypeScript users** — install the exact npm tarball from a GitHub release; §1.4 shows the package entry point.
* **C / other-language users** — start at §1.2 for a taste of the C ABI (`src/shared_lib/celestial.h`), then §9/§10 for prebuilt shared libraries.
* **Contributors** — `AGENTS.md` at the repository root is the single source of truth for build, test, lint, and code-style conventions.

## 1. Quick Start

### 1.1. From C++ (header-only)

No build step: point the compiler at the headers and call. Query the UT1 moment of a Jieqi (节气):

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
# The headers include each other by basename, so pass all three include dirs
clang++ -std=c++23 -I src/astro -I src/calendar -I src/util quickstart.cpp -o quickstart
./quickstart
# 冬至 2026 (UT1): 2026-12-21, day fraction 0.868205
```

The Jieqi query API lives in `src/calendar/jieqi.hpp` (`jieqi_ut1_moment`, `jieqi_jde`, `JieqiGenerator`). The rest of the library is organized the same way — self-contained headers under `src/astro/`, `src/calendar/`, and `src/util/`.

### 1.2. From C and other languages (the C ABI)

The same query across the C ABI (`src/shared_lib/celestial.h`), consumable from C, ctypes, and any FFI:

```c
#include <stdio.h>

#include "celestial.h"

int main(void) {
  char name[16]; /* index 21 = 冬至 in the to_index order (0 = 立春) */
  if (!get_jieqi_name(21, name, sizeof name)) return 1;
  const JieqiMomentQuery m = query_jieqi_moment(2026, 21);
  if (!m.valid) return 1;
  printf("%s (UT1): %d-%02u-%02u, day fraction %.6f\n", name, m.y, m.m, m.d, m.frac);
  return 0;
}
```

Build the shared library first (§4, or download a prebuilt one — §9/§10), then:

```sh
cc quickstart.c -I src/shared_lib -L build/shared_lib -lcelestial_calendar -Wl,-rpath,build/shared_lib -o quickstart_c
./quickstart_c
# 冬至 (UT1): 2026-12-21, day fraction 0.868205
```

With a downloaded prebuilt artifact (§9/§10) instead, point at its packaged layout — headers under `<artifact>/include`, the library under `<artifact>/lib`:

```sh
cc quickstart.c -I <artifact>/include -L <artifact>/lib -lcelestial_calendar -Wl,-rpath,<artifact>/lib -o quickstart_c
```

(On Windows, link against the import library instead and keep the DLL next to the executable.)

### 1.3. From Python

Install the wheel for your platform from the GitHub release, then import the flat API:

```sh
python -m pip install "./celestial_calendar-<version>-py3-none-<platform>.whl"
```

```python
import celestial_calendar as celestial

ut1 = celestial.CivilDateTime(2026, 8, 16, 0.5)
jde = celestial.ut1_to_jde(ut1)
winter_solstice = celestial.jieqi_moment(2026, celestial.Jieqi.DONGZHI)
```

Python 3.11 or newer is supported. Each wheel owns its native library; it neither searches the system nor downloads a
fallback at import time. Public calls use enums, frozen dataclasses, ordinary scalars and tuples rather than exposing
the underlying ctypes protocol. See `bindings/python/README.md` for the package contract.

### 1.4. From JavaScript or TypeScript

The package is not yet published to the npm registry. Download and extract `celestial-wasm.zip` from the matching
GitHub release, then install its exact npm tarball:

```sh
npm install ./0xf3cd-celestial-<version>.tgz
```

Initialize the package-owned WebAssembly module once, then use the synchronous APIs:

```js
import * as celestial from "@0xf3cd/celestial";

await celestial.init();
const moon = celestial.moon.illumination(2448724.5);
const lichun = celestial.jieqi.moment(2026, 0);
```

Node 22 or newer is supported; the browser package is tested on Chrome. The bundled declarations preserve the
JD/JDE/UT1 distinctions and expose enum-like inputs as string unions. See
`bindings/javascript/README.md` for the complete package contract.

## 2. Features

* Conversions between Gregorian, Lunar, and Ganzhi dates (公历、阴历、干支历之间的转换)
* Accurate Jieqi moment queries (查询某一年的某节气的具体时刻)
* Sunrise, sunset, transit, twilight, and polar day/night queries, within ±2 min of external references (USNO / NOAA / JPL DE) (日出日落、中天、曙暮光、极昼极夜)
* Geocentric apparent positions: ecliptic coordinates for the Sun and Moon, equatorial for the Sun, plus New Moon moments (日月视位置与合朔)
* Equation of time and local apparent solar time (均时差与真太阳时)
* Time scales and time-related quantities: UT1 / UTC / TT with leap seconds and ΔT, Julian Day, sidereal time, obliquity, nutation (时标转换、儒略日、恒星时、黄赤交角、章动)
* A C ABI shared library (`src/shared_lib/celestial.h`), so the library is consumable from other languages (C 接口动态库)
* Native Python wheels for manylinux x86_64/aarch64, macOS arm64, and Windows AMD64

The supported year range of lunar conversions depends on the algorithm: 1901–2099 for algo1 (Hong Kong Observatory data), 1600–2199 for algo3 (baked table), and 410–2500 for algo2 (computed from VSOP87D / ELP2000-82B — that window is a convention rather than a limit of the method, and it is enforced: years outside it are rejected; the 2500 ceiling comes from the #139 error budget). In C++ the bounds are the `START_YEAR` / `END_YEAR` constants of each `calendar::lunar::algoN`; the C ABI and JavaScript package expose all three, and `get_supported_lunar_year_range` reports their bounds.

## 3. Requirements

* C++ Compiler that supports C++23
  * CI builds it with clang++ 22 on Linux and Windows, the Apple clang in Xcode 26 on macOS, and g++ 14 on Linux. Older compilers may work; nothing checks them.
* CMake >=3.22, and make
* Python 3, mostly for build automation
  * Install dependencies: `python3 -m pip install -r Requirements.txt`
  * A distro-packaged Python (Debian, Ubuntu, ...) refuses that install under PEP 668. Work in a virtual environment there: `python3 -m venv .venv && .venv/bin/python project.py --all`. `--setup` installs nothing when the dependencies are already present, so an interpreter that already has them is fine as well.
  * `Requirements.txt` covers the build/test automation only. The linters come separately (see §8), and the notebooks and crawlers under `statistics/` need `python3 -m pip install -r Requirements-statistics.txt`

### 3.1. Prebuilt Native Archives

The supported column is the consumer compatibility promise; the measured column is an audit record and does not lower
that support floor. Linux records the greatest GLIBC and GLIBCXX requirements, macOS records the Mach-O deployment
target, and Windows records the Visual C++ runtime linkage. CI writes the same values to `build_info.json`, rejects a
measured version above its supported counterpart, and checks release artifacts against this matrix. Linux assumes the
standard `libstdc++.so.6` and `libgcc_s.so.1` system runtimes are present. No Windows OS version floor is declared.

<!-- native-runtime-matrix -->
| Artifact | Supported runtime | Measured artifact property |
|---|---|---|
| `linux_amd64` | `glibc=2.28, glibcxx=3.4.21` | `glibc=2.26, glibcxx=3.4.21` |
| `linux_arm64` | `glibc=2.28, glibcxx=3.4.21` | `glibc=2.17, glibcxx=3.4.21` |
| `macos_arm64` | `macos=14.0` | `macos=14.0` |
| `windows_x86_64` | `windows=not_declared` | `msvc_runtime=static` |

## 4. How to Build

### 4.1. On Unix-like Systems (macOS / Ubuntu / Debian ...)

Follow these steps to set up, build, and test the project on Unix-like systems. Ensure you have a C++23 compatible compiler installed.

Before building the project, you should specify the compiler to use. For example, to use `clang++`, run:

```sh
# Specify the compiler that supports C++23 on your platform
export CXX=clang++ # Change this to fit your platform

# Make the automation script executable
chmod +x project.py

# Install dependencies and ensure the C++ compiler works
./project.py --setup

# Build the project
./project.py --cmake --build

# Run tests
./project.py --test

# Randomized tests use a seeded engine (default 42); override to replay or explore
CELESTIAL_TEST_SEED=123 ./project.py --test

# Or, run all above together to build and test
./project.py --all

# Run the benchmarks (opt-in; not part of --all)
./project.py --bench

# Clean up builds
./project.py --clean

# More usages
./project.py --help
```

### 4.2. On Windows

Follow these steps to set up, build, and test the project on Windows. Ensure you have a C++23 compatible compiler installed.

Windows carries neither LLVM nor `make` out of the box, and the build drives CMake through the `Unix Makefiles` generator, so both are needed. Install them first — this is what CI does:

```powershell
choco install -y make llvm
```

Before building the project, you should specify the compiler to use. For example, to use `clang++`, run:

```powershell
# Specify the compiler that supports C++23 on your platform
$env:CXX = clang++
# CMake on Windows doesn't allow mixed use of compilers, so specify the LLVM C compiler as well, otherwise it may cause problems
$env:CC  = clang   

# Install dependencies and ensure the C++ compiler works
python3 ./project.py --setup

# Build the project
python3 ./project.py --cmake --build

# Run tests
python3 ./project.py --test

# Or, run all above together to build and test
python3 ./project.py --all

# Run the benchmarks (opt-in; not part of --all)
python3 ./project.py --bench

# Clean up builds
python3 ./project.py --clean

# More usages
python3 ./project.py --help
```

## 5. How the Library Is Verified

Correctness here is numerical, proven against external references. The test suite (`src/test/`) is data-driven: each golden dataset holds the library to reference values with a declared tolerance and a stated provenance, so every dataset stays regenerable and auditable. `src/test/jieqi_golden_test.cpp` is a representative example; the convention itself is documented in `AGENTS.md`.

The external oracles the library is held against include:

* **JPL Horizons** (DE441) — Sun/Moon apparent positions and Jieqi crossings, collected by the crawlers under `statistics/` (`moon_horizons_crawler.py`, `sun_jieqi_golden_crawler.py`).
* **Hong Kong Observatory almanac** — published Jieqi wall clocks (2022–2028); the Jieqi chain is held to within 60 s of them, a budget that mostly absorbs HKO's own minute rounding (`automation/jieqi_table.py`, run by `./linter.py --jieqi-table`).
* **ytliu0's ChineseCalendar** — an independent lunar-calendar year table, pinned by commit, as the golden oracle for the baked lunar algorithm (`src/test/lunar/algo3_ytliu0_golden_test.cpp`).
* **Observed ΔT** — the UT1 ↔ TT conversion is anchored to observed values (NASA eclipse ΔT table, USNO observations, Stephenson & Morrison), not to the library's own fitted ΔT model (`src/test/astro/julian_day_test.cpp`).
* **Sunrise/sunset** — held within ±2 min of USNO / NOAA / JPL DE references (§2).

The `statistics/` directory holds the crawlers that regenerate these datasets and the evaluation notebooks behind them (`python3 -m pip install -r Requirements-statistics.txt`).

## 6. WebAssembly and npm Package

`python3 toolbox/build_wasm.py` compiles the shared-library sources into a browser/Node ES module, emitting `build/wasm/celestial-jieqi.mjs` + `celestial-jieqi.wasm`. It needs an emsdk checkout — point at it with `--emsdk` or the `$EMSDK` environment variable.

The module contains all 29 stable exports in `celestial.h`; `@0xf3cd/celestial` wraps them as the `config`,
`time`, `sun`, `moon`, `jieqi`, and `lunar` namespaces. Raw heap pointers, count/fill protocols, sret layouts,
and `last_error` stay internal. `python3 toolbox/build_npm.py` stages and packs the exact eight-file npm tarball
from the generated module and the version in `project.py`.

CI builds the module and package on an independent leg (`wasm.yml`). Its `celestial-wasm` artifact contains the
raw `.mjs/.wasm` pair, the exact npm tarball, `npm-pack.json`, and a SHA-256 sidecar; the release flow (§10) picks
up that artifact unchanged. The same leg reconciles all 29 signatures and 16 layouts, replays the 389-point
native-generated golden dataset, installs the tarball in unrelated Node consumers, compiles its TypeScript
declarations, and runs an Astro/Vite production smoke in Chrome.

## 7. Export the Jieqi Table (JSON)

`toolbox/jieqi_table.py` turns `query_jieqi_moment` into one static JSON table (#164), for
consumers that only need "which Jieqi is now, and how many days to the next" without linking
the library. Build first (`./project.py --build`), then:

```sh
# The default table: 1950–2051 inclusive (24 × 102 = 2448 entries), to stdout
python3 ./toolbox/jieqi_table.py

# Write to a file, or choose another window within [401, 9999]
python3 ./toolbox/jieqi_table.py -o jieqi.json --start-year 2000 --end-year 2031
```

The contract of the emitted table:

* One entry per Jieqi moment: `{year, idx, name_zh, unix_ms, iso_utc}`. `idx` counts from
  立春 = 0 (the ABI's `to_index` order); `name_zh` echoes the ABI's own `get_jieqi_name`,
  so a consumer can cross-check the mapping instead of trusting it.
* Entries are sorted by moment, strictly increasing — within a calendar year the index order
  runs 22, 23, 0, …, 21 (小寒/大寒 lead the year), and the sort is done here, once.
* `year` is the attribution year — the `Y` passed to the query. Over the default window all
  24 crossings of `Y` land inside calendar year `Y`, and the gate holds this. At high years
  it stops holding — observed at year 9999, whose 小寒 lands on 9998-12-31 — and then `year`
  and the year inside `iso_utc` diverge.
* The default window ends at 2051, one tail-margin year, so every moment of 1950–2050 has
  its successor inside the table.
* Timescale is **UT1**; how far that sits from UTC depends on the era, and each table states
  it per era in its own `timescale_note`.
* Sub-millisecond precision is truncated, never rounded; `iso_utc` renders the same
  millisecond as `unix_ms`. Output carries no generation timestamp — two runs of the same
  commit are byte-identical.

The table is held to all of the above (plus HKO almanac anchors and an independent
re-derivation through `statistics/common.py`) by `./linter.py --jieqi-table`.

## 8. Linters and Static Analysis

The project is written in C++, and automated with Python scripts.

For C++ codes, `clang-tidy` is used; For Python codes, `ruff` is used.

Neither is part of `Requirements.txt` — install them directly:

```sh
python3 -m pip install ruff

# clang-tidy: any 22.1.x will do. CI uses the 22.1.2 that ships in its runner image, and pip
# does not carry that exact patch release -- take the nearest one, or your distro's package.
python3 -m pip install clang-tidy==22.1.8      # or your distribution's clang-tidy-22
```

`clang-tidy` runs with `WarningsAsErrors: '*'`, so its version is pinned deliberately — a newer
one ships new checks that flag pre-existing code. Point `CLANG_TIDY` at the binary you want if
`clang-tidy` on your `PATH` is a different major; it has to match the vendored
`run-clang-tidy.py`, or you are measuring with a different ruler than CI (AGENTS.md gotcha 9).

The check configuration for `clang-tidy` is placed at `.clang-tidy`.

### 8.1. On Unix-like Systems (macOS / Ubuntu / Debian ...)

```sh
# Run ruff
./linter.py --ruff

# Run clang-tidy
./linter.py --clang-tidy
```

### 8.2. On Windows

```powershell
# Run ruff
python3 ./linter.py --ruff

# Run clang-tidy
python3 ./linter.py --clang-tidy
```

## 9. Download Build Artifacts

There are basically two ways to download:

### 9.1. From GitHub Web UI

* Open the native, [WASM](https://github.com/0xf3cd/celestial-calendar/actions/workflows/wasm.yml), or
  [Python wheel](https://github.com/0xf3cd/celestial-calendar/actions/workflows/python-wheel.yml) workflow.
* Download from the completed run that built your commit.
  
### 9.2. Use `toolbox/artifact_downloader.py`

* Install dependencies: `python3 -m pip install -r Requirements.txt`
* Set environment variable `GITHUB_TOKEN` to your GitHub personal access token, because it is needed to download artifacts
* Run `toolbox/artifact_downloader.py`

  ```sh
  # Ensure env var `GITHUB_TOKEN` is correctly set
  echo $GITHUB_TOKEN     # Unix-like platforms
  echo $env:GITHUB_TOKEN # Windows powershell

  # Download artifacts from a given run to the specified dir
  python3 ./toolbox/artifact_downloader.py -id <run-id> -s <directory>

  # Download the exact native, WASM, and Python inventories built from HEAD
  python3 ./toolbox/artifact_downloader.py -s <directory>

  # Same, and unzips them
  python3 ./toolbox/artifact_downloader.py -s <directory> --unzip

  # More usages
  python3 ./toolbox/artifact_downloader.py --help

  # Or run it as a Python module from root dir
  python3 -m toolbox.artifact_downloader --help
  ```

## 10. Download Release

There are basically two ways to download:

### 10.1. From GitHub Web UI

* Go to [Releases](https://github.com/0xf3cd/celestial-calendar/releases)
* Download the native/WASM archives, direct Python wheels with SHA-256 sidecars, and source code

### 10.2. Use `toolbox/release_downloader.py`

* Install dependencies: `python3 -m pip install -r Requirements.txt`
* Set environment variable `GITHUB_TOKEN` to your GitHub personal access token, because it is needed to download assets
* Run `toolbox/release_downloader.py`

  ```sh
  # Ensure env var `GITHUB_TOKEN` is correctly set
  echo $GITHUB_TOKEN     # Unix-like platforms
  echo $env:GITHUB_TOKEN # Windows powershell

  # Download assets from the latest release to the specified dir
  python3 ./toolbox/release_downloader.py -s <directory>

  # More usages
  python3 ./toolbox/release_downloader.py --help

  # Or run it as a Python module from root dir
  python3 -m toolbox.release_downloader --help
  ```

## 11. TODO List

* C++20/23 features are not fully supported by the compilers...
  * Modules
  * Ranges and views (e.g. `std::views::enumerate`, `pairwise`...)
  * Use `std::generator` in Newton's method (moon_phase and jieqi).
  * Which of these a toolchain can actually compile: `./linter.py --features`. CI runs the same
    probe on libstdc++ / libc++ / MSVC STL and fails when a leg gains a feature the code is
    still hand-rolling around, so the list above cannot go quietly stale.
* DUT1 (i.e. UT1 - UTC) is not modelled
  * UTC became a first-class time scale in v0.4.0 (leap-second aware, `utc_to_tt` / `tt_to_utc`), but UT1 and UTC are still treated as interchangeable — the gap stays below 0.9 s while leap seconds are in force.

## 12. References

* [Julian Day Numbers](https://quasar.as.utexas.edu/BillInfo/JulianDatesG.html)
* [Definitions of Systems of Time](https://www.cnmoc.usff.navy.mil/Our-Commands/United-States-Naval-Observatory/Precise-Time-Department/The-USNO-Master-Clock/Definitions-of-Systems-of-Time/)
* [USNO Delta T Values](https://maia.usno.navy.mil/ser7/deltat.data)
* [SOFA Library (ANSI C)](https://www.iausofa.org/2023-10-11c)
* [Stephenson, Morrison & Hohenkerk, "Measurement of the Earth's rotation: 720 BC to AD 2015" (Proc. R. Soc. A 472)](https://doi.org/10.1098/rspa.2016.0404)
* [vsop87c](https://github.com/hongzhen/vsop87c)
* [PyMeeus](https://github.com/architest/pymeeus)
* [meeus-elp82](https://www.celestialprogramming.com/meeus-elp82.html)
* [AA+ v2.55 A class framework for Computational Astronomy](http://www.naughter.com/aa.html)
* [农历24节气算法](https://www.cnblogs.com/qintangtao/archive/2013/03/04/2942245.html)
* [算法系列之十八：用天文方法计算二十四节气（上）](https://github.com/leetcola/nong/wiki/算法系列之十八：用天文方法计算二十四节气（上）)
* [算法系列之十八：用天文方法计算二十四节气（下）](https://github.com/leetcola/nong/wiki/算法系列之十八：用天文方法计算二十四节气（下）)
* [算法系列之十九：用天文方法计算日月合朔（新月）](https://github.com/leetcola/nong/wiki/算法系列之十九：用天文方法计算日月合朔（新月）)
* [历书科普问题解答 - 中国科学院紫金山天文台](http://www.pmo.cas.cn/xwdt2019/kpdt2019/202203/t20220317_6399980.html)
* [农历编算法则](https://ytliu0.github.io/ChineseCalendar/rules_simp.html)
* [ytliu0 / ChineseCalendar](https://github.com/ytliu0/ChineseCalendar)
* [JPL Horizons](https://ssd.jpl.nasa.gov/horizons/)
* [Hong Kong Observatory — 24 Solar Terms](https://www.hko.gov.hk/en/gts/astronomy/Solar_Term.htm)
