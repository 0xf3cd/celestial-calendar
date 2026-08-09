# Celestial Calendar

> A C++23-style library that performs astronomical calculations and date conversions among various calendars, including Gregorian, Lunar, and Chinese Ganzhi calendars.

## 1. Features

* Conversions between Gregorian, Lunar, and Ganzhi dates (公历、阴历、干支历之间的转换)
* Accurate Jieqi moment queries (查询某一年的某节气的具体时刻)
* Sunrise, sunset, transit, twilight, and polar day/night queries, within ±2 min of external references (USNO / NOAA / JPL DE) (日出日落、中天、曙暮光、极昼极夜)
* Geocentric apparent positions: ecliptic coordinates for the Sun and Moon, equatorial for the Sun, plus New Moon moments (日月视位置与合朔)
* Equation of time and local apparent solar time (均时差与真太阳时)
* Time scales and time-related quantities: UT1 / UTC / TT with leap seconds and ΔT, Julian Day, sidereal time, obliquity, nutation (时标转换、儒略日、恒星时、黄赤交角、章动)
* A C ABI shared library (`src/shared_lib/celestial.h`), so the library is consumable from other languages (C 接口动态库)

The supported year range of lunar conversions depends on the algorithm: 1901–2099 for algo1 (Hong Kong Observatory data), 1600–2199 for algo3 (baked table), and 410–2500 for algo2 (computed from VSOP87D / ELP2000-82B — that window is a convention rather than a limit of the method, and it is enforced: years outside it are rejected; the 2500 ceiling comes from the #139 error budget). In C++ the bounds are the `START_YEAR` / `END_YEAR` constants of each `calendar::lunar::algoN`; the C ABI exports algo1 and algo2, and `get_supported_lunar_year_range` reports their bounds.

## 2. Requirements

* C++ Compiler that supports C++23
  * Currently, clang++ (LLVM 18) is able to compile the project on macOS, Windows, and Linux. g++ 14 also works.
* CMake >=3.22, and make
* Python 3, mostly for build automation
  * Install dependencies: `python3 -m pip install -r Requirements.txt`
  * A distro-packaged Python (Debian, Ubuntu, ...) refuses that install under PEP 668. Work in a virtual environment there: `python3 -m venv .venv && .venv/bin/python project.py --all`. `--setup` installs nothing when the dependencies are already present, so an interpreter that already has them is fine as well.
  * `Requirements.txt` covers the build/test automation only. The linters come separately (see §4), and the notebooks and crawlers under `statistics/` need `python3 -m pip install -r Requirements-statistics.txt`

## 3. How to Build

### 3.1. On Unix-like Systems (macOS / Ubuntu / Debian ...)

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

### 3.2. On Windows

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

## 4. Linters and Static Analysis

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

### 4.1. On Unix-like Systems (macOS / Ubuntu / Debian ...)

```sh
# Run ruff
./linter.py --ruff

# Run clang-tidy
./linter.py --clang-tidy
```

### 4.2. On Windows

```powershell
# Run ruff
python3 ./linter.py --ruff

# Run clang-tidy
python3 ./linter.py --clang-tidy
```

## 5. Download Artifacts (Shared Libs)

There are basically two ways to download:

### 5.1. From GitHub Web UI

* Go to [Action Page](https://github.com/0xf3cd/celestial-calendar/actions/workflows/build_and_test.yml)
* Download from the latest completed run
  
### 5.2. Use `toolbox/artifact_downloader.py`

* Install dependencies: `python3 -m pip install -r Requirements.txt`
* Set environment variable `GITHUB_TOKEN` to your GitHub personal access token, because it is needed to download artifacts
* Run `toolbox/artifact_downloader.py`

  ```sh
  # Ensure env var `GITHUB_TOKEN` is correctly set
  echo $GITHUB_TOKEN     # Unix-like platforms
  echo $env:GITHUB_TOKEN # Windows powershell

  # Download artifacts from a given run to the specified dir
  python3 ./toolbox/artifact_downloader.py -id <run-id> -s <directory>

  # Download artifacts from the successful run that built HEAD, to the specified dir
  python3 ./toolbox/artifact_downloader.py -s <directory>

  # Same, and unzips them
  python3 ./toolbox/artifact_downloader.py -s <directory> --unzip

  # More usages
  python3 ./toolbox/artifact_downloader.py --help

  # Or run it as a Python module from root dir
  python3 -m toolbox.artifact_downloader --help
  ```

## 6. Download Release

There are basically two ways to download:

### 6.1. From GitHub Web UI

* Go to [Releases](https://github.com/0xf3cd/celestial-calendar/releases)
* Download the assets and source codes

### 6.2. Use `toolbox/release_downloader.py`

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

## 7. TODO List

* C++20/23 features are not fully supported by the compilers...
  * Modules
  * Ranges and views (e.g. `std::views::enumerate`, `pairwise`...)
  * Use `std::generator` in Newton's method (moon_phase and jieqi).
  * Which of these a toolchain can actually compile: `./linter.py --features`. CI runs the same
    probe on libstdc++ / libc++ / MSVC STL and fails when a leg gains a feature the code is
    still hand-rolling around, so the list above cannot go quietly stale.
* DUT1 (i.e. UT1 - UTC) is not modelled
  * UTC became a first-class time scale in v0.4.0 (leap-second aware, `utc_to_tt` / `tt_to_utc`), but UT1 and UTC are still treated as interchangeable — the gap stays below 0.9 s while leap seconds are in force.

## 8. References

* [Julian Day Numbers](https://quasar.as.utexas.edu/BillInfo/JulianDatesG.html)
* [Definitions of Systems of Time](https://www.cnmoc.usff.navy.mil/Our-Commands/United-States-Naval-Observatory/Precise-Time-Department/The-USNO-Master-Clock/Definitions-of-Systems-of-Time/)
* [USNO Delta T Values](https://maia.usno.navy.mil/ser7/deltat.data)
* [SOFA Library](https://www.iausofa.org/2021_0512_C)
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
