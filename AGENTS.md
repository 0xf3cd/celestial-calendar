# Agent Instructions — CelestialCalendar

## Project Overview

**CelestialCalendar** — A C++23 astronomical calendar library that converts among Gregorian, Lunar, and Chinese Ganzhi calendars, and computes accurate Jieqi (solar term) moments.

Repository: `https://github.com/0xf3cd/celestial-calendar`  
License: GNU General Public License v3.0

## Tech Stack

- **C++23** — Core library (`src/`)
- **CMake ≥ 3.22** — Build system
- **Python 3** — Build/test automation (`project.py`, `linter.py`, `automation/`)
- **GoogleTest** — Fetched at configure time by CMake (`src/test/CMakeLists.txt`)
- **clang-tidy** — C++ static analysis
- **ruff** — Python linting/formatting

## Build, Test, and Run

Preferred entry point is `project.py`. Set `CXX` (and `CC` on Windows) to a C++23-capable compiler before running.

```sh
# macOS / Linux example
export CXX=clang++

# Full setup + configure + build + test
./project.py --all

# Individual steps
./project.py --setup      # Install Python deps and verify toolchain
./project.py --cmake      # Run CMake configuration
./project.py --build      # Build shared library and tests
./project.py --test       # Run all tests
./project.py --test -k integration -v 1  # Filtered, verbose tests
./project.py --clean      # Remove build/ directory
```

Windows PowerShell:

```powershell
$env:CXX = "clang++"
$env:CC  = "clang"
python3 ./project.py --all
```

### Direct CMake (optional)

```sh
cmake -S src -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build
```

## Code Style and Quality Conventions

### C++

- **Standard:** C++23. clang++ (LLVM 18+) and g++ 14+ are supported.
- **Identifier style** (enforced by `.clang-tidy`):
  - Functions / variables / parameters / members / namespaces: `lower_case`
  - Classes / structs: `CamelCase`
  - Enums and global constants: `UPPER_CASE`
  - Enum constants: `UPPER_CASE`
- **Formatting:** 2-space indentation, trailing-return-type style for functions (`auto name() -> type`).
- **Headers:** `#pragma once`; every header starts with the project GPL license blurb.
- **Compiler flags:** `-Wall -Wextra -Werror -Wpedantic -Wnull-dereference -Wunreachable-code -O2`.
- **Namespaces:** Code lives under descriptive namespaces (e.g., `lib::`, `astro::`, `calendar::`).

### Python

- **Linter/formatter:** ruff (`./linter.py --ruff` or `ruff check .` / `ruff format .`)
- **Config:** `.ruff.toml`
  - Line length: 120
  - Indent: 2 spaces
  - Selected rules: `E`, `F`, `B`, `Q`
  - Quote style: double
- Run `./linter.py --all` to check both C++ and Python.

### Static Analysis

- `clang-tidy` checks are strict; `WarningsAsErrors: '*'` is enabled.
- Selected check suites: `clang-analyzer-*`, `performance-*`, `portability-*`, `bugprone-*`, `cppcoreguidelines-*`, `readability-*`, `google-*`, `modernize-*`.
- Several checks are explicitly disabled in `.clang-tidy` (magic numbers, identifier length, etc.); consult the file before adding `NOLINT` suppressions.

## Project Layout

```
src/
  astro/        Astronomical algorithms (VSOP87D, ELP2000-82B, Sun, Moon, ΔT, Julian Day, ...)
  calendar/     Calendar logic: datetime, lunar conversion algorithms, Jieqi
  shared_lib/   C++ shared-library wrapper over core algorithms
  test/         GoogleTest-based tests (auto-discovered by CMake)
  util/         Utility headers (hash, cache, random, YMD, ...)
automation/     Python modules used by project.py and linter.py
toolbox/        Helper scripts for artifacts, releases, build info
```

## Project-Specific Rules and Gotchas

1. **Two agent docs, different jobs.** `CLAUDE.md` = code style & engineering philosophy (numerical-correctness discipline, the author's voice); this `AGENTS.md` = ops conventions (build/test/lint commands, project layout, CI gotchas). On style questions, `CLAUDE.md` wins.
2. **C++23 gaps:** The README explicitly notes that modules, ranges/views, and `std::generator` are intentionally avoided because compiler support is uneven across CI platforms. Do not introduce them without a strong compatibility justification.
3. **Tests are auto-discovered:** `src/test/CMakeLists.txt` globs every `*.cpp` under `src/test/` into a separate executable. Discovery timeout is 180s to accommodate slow Docker/ARM CI runners.
4. **Shared library target:** `src/shared_lib/CMakeLists.txt` builds `libcelestial_calendar` from `lib*.cpp`. Version is injected via the `BUILD_VERSION` environment variable (defaults to `0.0.0`).
5. **CI produces cross-platform artifacts:** GitHub Actions builds on macOS, Windows, and many Linux architectures via Docker+QEMU. Do not change compiler or Docker base images without checking matrix impact.
6. **Sensitive files:** Do not read or surface `.env`, `credentials.json`, or any file containing tokens/keys.
7. **`build/` is gitignored.** Generated artifacts and `compile_commands.json` live there; do not commit them.
8. **Use `project.py` for routine work.** It handles environment checks, version propagation, and test filtering consistently across platforms.

## Common Commands Reference

| Task | Command |
|------|---------|
| Full setup + build + test | `./project.py --all` |
| Configure only | `./project.py --cmake` |
| Build only | `./project.py --build` |
| Run tests, verbose | `./project.py --test -v 1` |
| Filtered tests | `./project.py --test -k <keyword>` |
| Clean | `./project.py --clean` |
| Python lint/format | `./linter.py --ruff` |
| C++ lint | `./linter.py --clang-tidy` |
| Show version | `./project.py --version` |
