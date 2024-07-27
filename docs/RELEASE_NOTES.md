# Release Notes

## [v0.0.0] - 2024-07-26

### Added

#### Astronomical Calculation
- Supported conversions between JD (Julian Day) and UT1, and conversions between JDE (Julian Ephemeris Day) and TT.
- Supported calculations of Delta T.
- Supported conversions between UT1 and TT time scales.
- Supported calculations of the Heliocentric Coordinate of Earth.
- Supported calculations of the Geocentric Coordinate of the Sun, with corrections (FK5 system correction, nutation correction, etc.).
- Added other math utilities, e.g., `Angle`.

#### Statistics
- Added `statistics`, for statistical analysis to evaluate different astronomical algorithms

#### Calendar
- Added `Datetime`, a constexpr struct to hold a date and an accurate time, representing a UT1 or UTC moment.
- Supported conversions between Lunar dates and Gregorian dates.
  - Currently, only Gregorian years between 1901 and 2099 are supported.
- Applied Newton's method to approximate the moment when the Sun reaches a certain longitude.
- Supported queries of the Jieqi (节气) moments in given Gregorian years.

#### Test
- Implemented unit tests with GTest, covering core functionalities.

#### Automation
- Implemented the `automation` Python package to manage the project in an automated manner.
- Added `project.py` as the entry point for building and testing the project.
- Created the `toolbox` folder, including `latest_build_downloader.py` to download the latest shared libraries.
