# Release Notes

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
