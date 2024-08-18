# Release Notes

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
