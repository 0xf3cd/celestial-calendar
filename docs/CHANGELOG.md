# Changelog

## [v0.2.0] - 2024-08-17

### Added

#### Calendar

- Supported more algorithms for lunar date conversion.
- Added more functions for Jieqi calculation.
- Used adaptive step size in Newton's method.

#### Utilities

- Supported hash and cache for arbitrary function, which is currently used in astro-calculation, jieqi calculation, and lunar date conversions.

#### Statistics

- Added a new notebook in folder `statistics`:
  - `lunar_calendar.ipynb` for inspecting the accurate lunar month start moments, also for inspecting the differences between algo1 and algo2.

#### Test

- Implemented unit tests with GTest, covering lunar date conversions using different algorithms.

#### Linter

- Added `.ruff.toml` which configures ruff. Also change Python codes as per the linter suggestions.

## [v0.1.0] - 2024-08-05

### Added

#### Astronomical Calculation

- Added theory ELP2000-82B (truncated version, from Jean Meeus, "Astronomical Algorithms", Second Edition, Chapter 47).
- Supported calculations of the Geocentric Coordinate of the Moon, with corrections (perturbation, nutation, ...).
- Applied Newton's method to approximate the moments of Sun-Moon conjunctions (i.e. New Moons).
- Added other math utilities, e.g., `Distance`.

#### Statistics

- Added more notebooks in folder `statistics`:
  - `moon_longitude.ipynb` for exploring Newton's Method on finding New Moon moments.
  - `new_moon.ipynb` to compare calculated New Moon moments with other data sources.

#### Test

- Implemented unit tests with GTest, covering ELP2000-82B, Moon position calculation, and Sun-Moon conjunction moment prediction.

#### Automation

- Added `linter.py` to run `ruff` and `clang-tidy`.
- Created the `toolbox` folder, including:
  - `release_downloader.py` to download the assets from latest release.

## [v0.0.0] - 2024-08-01

### Added

#### Astronomical Calculation

- Supported conversions between JD (Julian Day) and UT1, and conversions between JDE (Julian Ephemeris Day) and TT.
- Supported calculations of Delta T.
- Supported conversions between UT1 and TT time scales.
- Supported calculations of the Heliocentric Coordinate of Earth.
- Supported calculations of the Geocentric Coordinate of the Sun, with corrections (FK5 system correction, nutation correction, etc.).
- Added other math utilities, e.g., `Angle`.

#### Statistics

- Added folder `statistics`, for statistical analysis to evaluate different astronomical algorithms

#### Calendar

- Added `Datetime`, a struct to hold a date and an accurate time, representing a UT1 or UTC moment.
- Supported conversions between Lunar dates and Gregorian dates.
  - Currently, only Gregorian years between 1901 and 2099 are supported.
- Applied Newton's method to approximate the moment when the Sun reaches a certain longitude.
- Supported queries of the Jieqi (节气) moments in given Gregorian years.

#### Test

- Implemented unit tests with GTest, covering core functionalities.

#### Automation

- Implemented the `automation` Python package to manage the project in an automated manner.
- Added `project.py` as the entry point for building and testing the project.
- Created the `toolbox` folder, including:
  - `artifact_downloader.py` to download the latest shared libraries.
  - `build_info.py` to pack the platform, architecture, and shared lib info with a build.
  - `compiler_finder.py` to find the C and C++ compilers that satisfy a certain standard.
