# Celestial Calendar

> A C++23-style library that performs astronomical calculations and date conversions among various calendars, including Gregorian, Lunar, and Chinese Ganzhi calendars.

## 1. Features

* Conversions between Gregorian, Lunar, and Ganzhi dates (公历、阴历、干支历之间的转换)
* Accurate Jieqi moment queries (查询某一年的某节气的具体时刻)

## 2. Requirements

* C++ Compiler that supports C++23
  * Currently, clang++ (LLVM 18) is able to compile the project on macOS, Windows, and Linux. g++ 14 also works.
* CMake >=3.22, and make
* Python 3, mostly for build automation
  * Install dependencies: `python3 -m pip install -r Requirements.txt`

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

# Or, run all above together to build and test
./project.py --all

# Clean up builds
./project.py --clean

# More usages
./project.py --help
```

### 3.2. On Windows

Follow these steps to set up, build, and test the project on Windows. Ensure you have a C++23 compatible compiler installed.

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

# Clean up builds
python3 ./project.py --clean

# More usages
python3 ./project.py --help
```

## 4. Linters and Static Analysis

The project is written in C++, and automated with Python scripts.

For C++ codes, `clang-tidy` is used; For Python codes, `ruff` is used.

Ensure `clang-tidy` and `ruff` are installed by `python3 -m pip install -r Requirements.txt`.

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

  # Download artifacts from latest run to the specified dir
  python3 ./toolbox/artifact_downloader.py -s <directory>

  # Download artifacts from latest run to the specified dir and unzips them
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

  # Download assets from a given release to the specified dir
  python3 ./toolbox/release_downloader.py -id <release-id> -s <directory>

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
  * Ranges and views (e.g. cartesian_product...)
  * Use `std::generator` in Newton's method (syzygy and jiqei).
* Support conversions between UTC and UT1
  * Their differences are subtle and are ignored at this moment...

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
