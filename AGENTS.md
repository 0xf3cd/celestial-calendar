# Agent Instructions — CelestialCalendar

> **This file is the single source of truth (SSOT) for agent instructions.**
> `CLAUDE.md` is only an import pointer to this file. If anything elsewhere
> (README, PR comments, habit) conflicts with this file, this file wins — or fix
> the file, don't fork the convention.

## Project Overview

**CelestialCalendar** — A C++23 astronomical-calculation library that converts among
Gregorian, Lunar, and Chinese Ganzhi calendars, and computes accurate Jieqi (节气)
moments, sun/moon positions, and sunrise/sunset. 天文计算与历法转换.

Repository: `https://github.com/0xf3cd/celestial-calendar`
License: GNU General Public License v3.0

This file tells any AI assistant (Claude, Kimi, Codex, Gemini, ...) how to write code
**in the author's style**, so contributions stay indistinguishable from hand-written
code. For *style* questions, read a neighbouring header and imitate it rather than
inventing a convention.

## Human-in-the-loop (read first)

Stop and confirm with the author before proceeding whenever anything is unclear or
non-obvious — an ambiguous requirement, a design fork with no precedent in the code, an
algorithm/reference you're unsure of, an unexpected test failure, or any change whose
intent you can't verify from existing code. Prefer asking over guessing.

## Correctness is numerical, measured against references

This is precision astronomy, not vibes. Every algorithm traces to a named reference
(Meeus, VSOP87D, ELP2000-82B, SOFA, USNO ΔT — see README §References).

- **Name cross-formula physical constants** as `constexpr` (UPPER_CASE) — e.g.
  `SIDEREAL_RATE_DEG_PER_DAY`, shared between implementation and tests. Single-use
  coefficients of a cited reference polynomial stay **inline in the formula** with the
  `@ref` alongside (like `obliquity::mean`, `gen_eval_θ`, the ΔT segments): naming each
  coefficient of a quoted equation adds indirection without audit value. Dense coefficient
  tables (e.g. `MEEUS_NUTATION_COEFFS`) keep raw literals with a source comment.
  `.clang-tidy` disables the magic-number checks for exactly these two cases.
- Never "simplify" a formula or drop a term for cleaner code — the constants ARE the algorithm.
- Correctness is proven by **golden datasets**: tests hold high-precision reference values
  and `ASSERT_NEAR` to a per-column tolerance. New numerics ⇒ add a reference dataset + tolerance.
- **Never loosen an existing tolerance on your own.** Before touching a tolerance: talk to the
  author, debug it carefully, and confirm case-by-case that it's an algorithm/reference issue,
  not a code bug. The default is to surface the discrepancy, not to make the test green.
- Cite the source of any new algorithm / coefficients in a comment.

## Tech Stack

- **C++23** — Core library (`src/`); clang++ (LLVM 18+) and g++ 14+ are supported.
- **CMake ≥ 3.22** — Build system.
- **Python 3** — Build/test/lint automation (`project.py`, `linter.py`, `automation/`,
  `toolbox/`). All build/lint/CI is Python-orchestrated.
- **GoogleTest** — Fetched at configure time by CMake (`src/test/CMakeLists.txt`).
- **clang-tidy** — C++ static analysis; **ruff** — Python linting/formatting.

## Build, Test, and Lint

Preferred entry point is `project.py`. Set `CXX` (and `CC` on Windows) to a C++23-capable
compiler before running.

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

### Lint

- `clang-tidy` runs with **`WarningsAsErrors: '*'`** — zero warnings. `.clang-tidy` is the
  config; respect its deliberate disables (magic-numbers, identifier-length, identifier-naming)
  instead of "fixing" code to satisfy a disabled check.
  Selected check suites: `clang-analyzer-*`, `performance-*`, `portability-*`, `bugprone-*`,
  `cppcoreguidelines-*`, `readability-*`, `google-*`, `modernize-*`.
- Python: ruff (`./linter.py --ruff` or `ruff check .` / `ruff format .`), config `.ruff.toml`
  (line length 120, 2-space indent, rules `E`/`F`/`B`/`Q`, double quotes).
- Run `./linter.py --all` to check both C++ and Python.

## Code Style

### Naming (SSOT: `.clang-tidy` CheckOptions)

`lower_case` — variables / functions / params / members / methods / namespaces.
`CamelCase` — class / struct.  `UPPER_CASE` — global constants / enums / enum constants.

### Header-only, and the code reads like the maths

Header-only is a **deliberate design choice** — the no-link, orthogonal, self-contained feel
is intentional. Keep it. That buys a discipline:

- All logic in `src/**/*.hpp` with `#pragma once`; every function `inline`, shared constants
  `inline constexpr`. `.cpp` exists only in `shared_lib/` for the C-ABI exports. New logic → a header.
- **No `using namespace` and no namespace-scope `using` declarations in headers** — they leak
  into every including TU and cause conflicts / ambiguity. Put `using` inside function bodies
  (as `sun.hpp` already does with `using namespace astro::toolbox::literals;`) or fully-qualify.
  Existing violations tracked in #51.
- **Nested `lower_case` namespaces** by domain (`astro::earth::nutation`,
  `lib::`, `calendar::`); close with a `} // namespace …` comment.
- **Trailing return types**: `inline auto f(const double jde) -> SphericalCoordinate`.
- **Unicode math identifiers are deliberate** (`λ`, `β`, `Δψ`, `Ω`, `Mp`). Name symbols as the
  papers do; do NOT ASCII-ise them.
- **Strong physical types** over bare doubles (`Angle<RAD>`, `Distance<AU>`, `SphericalCoordinate`)
  with designated initializers (`.λ = …`). Keep units in the type system.
- `const`-correct; modern C++ (`<ranges>`, `<span>`, `std::array`).
- 2-space indentation. Compiler flags: `-Wall -Wextra -Werror -Wpedantic -Wnull-dereference
  -Wunreachable-code -O2`.
- Doxygen `/** @brief / @param / @return / @note / @ref */` on public functions; inline `//`
  comments explain the physics, not the syntax — with order-of-magnitude arguments
  ("ΔT ≈ 69 s ≈ 0.29° of rotation"), not bare claims ("this is accurate"). `@ref` cites the
  exact formula/table number of the named reference.

### Modern C++ posture

- **Immutability by default**: locals are `const`; value types (`Angle`, `Distance`) are
  immutable — operations return new values. No setters anywhere.
- **Composition over inheritance** — in fact, no inheritance in the library at all: structs
  are plain data carriers; "polymorphism" is values + table lookup
  (`nutation::Model` → `find_model` → `std::span`), never vtables.
- **Free functions over classes**: algorithms are `inline` free functions in nested
  namespaces; classes/structs exist only to carry data.
- **Declarative over imperative**: ranges pipelines (`| std::views::transform` → `std::reduce`)
  describe *what*, not *how*.
- **Functional core**: algorithms are pure functions — no global or mutable shared state,
  same inputs ⇒ same outputs (this is what makes golden-dataset testing meaningful).
  Higher-order functions where natural (`gen_eval_θ(jc)` returns the θ-evaluator closure);
  map/fold pipelines over manual loops; data and behavior stay separate (plain structs +
  free functions over them); enum + table lookup is the sum-type idiom (`nutation::Model`
  → `find_model`), not vtables.
- **Compile-time over runtime**: `constexpr` wherever the language allows (but don't force it —
  `std::cos` / `std::pow` / `std::remainder` are only constexpr from C++26); constrain
  templates with `requires` clauses, never SFINAE.
- **Names as contracts**: parameter names carry semantics that make misuse visible at the
  call site — e.g. the `jd_ut1` / `jde_tt` suffixes are the UT1/TT guard (issue #41).
- **Suppressions carry reasons**: every `NOLINT` / `NOLINTNEXTLINE` has a trailing comment
  explaining why the warning does not apply here.

### Small functions, honest errors, no premature abstraction

- **Functions stay short and single-purpose** — compose thin layers that each do one thing
  (`greenwich_mean` → `greenwich_apparent` → `local_apparent`). The doc comment is routinely
  longer than the function body: the physics and the citation are the hard part, the code is
  the easy part. If a function needs a long body, split it before commenting around it.
- **Fail fast with informative errors**: throw with context-rich messages
  (`throw std::out_of_range { std::vformat("Year {} is not supported …", …) }`), never
  silently clamp, swallow, or return a sentinel.
- **KISS / minimal abstraction**: three similar lines beat a premature abstraction. Generalize
  when a second or third real caller demands it, not in anticipation.

### File header

Every file opens with the full GPL-3.0 block comment (copy from any src file), verbatim except
the year: `Copyright (C) <year> Ningqi Wang (0xf3cd)`. New files use the current year; never
change an existing file's year. GPL-3.0 project — keep the header.

## Tests

- GoogleTest `TEST(Suite, Case)` in `src/test/**`, mirroring `src/**`
  (`astro/earth.hpp` → `test/astro/earth_test.cpp`), namespace `…::test`.
- **Tests are auto-discovered:** `src/test/CMakeLists.txt` globs every `*.cpp` under `src/test/`
  into a separate executable. Discovery timeout is 180s to accommodate slow Docker/ARM CI runners.
- Data-driven: inline a column-aligned dataset of reference values (whitespace-padded,
  right-aligned columns like `earth_test.cpp`); `ASSERT_NEAR` with a per-column tolerance;
  header-comment the columns.
- **Provenance is mandatory**: every golden dataset states where its values came from —
  book example number (tests mirror worked examples value-by-value, e.g. Meeus Example 12.a),
  or the external source + collection date + generation seed (e.g. "USNO API, 2026-07-19,
  seed 42") — plus the rationale for the tolerance (measured margin, model gap). A dataset
  must be regenerable and auditable by a reader.
- Dataset **input columns must reproduce the reference source's actual inputs** — including
  any quantization the source applied (e.g. USNO evaluating LAST at 4-decimal longitudes).
  Otherwise tolerances silently absorb the mismatch and the dataset loses discriminating power.

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

1. **Single source of truth:** this `AGENTS.md` is the only agent-instruction file with
   content; `CLAUDE.md` just imports it. Don't create new per-tool instruction files —
   extend this one.
2. **C++23 features — "not yet", not "never":** basic C++20 ranges/views are in active use
   (`std::views::transform` etc.). Features the weakest CI toolchain doesn't support yet —
   currently **modules**, `std::generator`, and C++23 ranges additions like
   `std::views::cartesian_product` / `pairwise` — wait for compiler support, then get adopted
   (README §7 tracks the wishlist). Before using one, check support across the whole CI matrix
   (Apple Clang / clang 18 / gcc 14 / MSVC STL / Docker ARM) and drop a note when graduating
   a feature off this list.
3. **Shared library target:** `src/shared_lib/CMakeLists.txt` builds `libcelestial_calendar`
   from `lib*.cpp`. Version is injected via the `BUILD_VERSION` environment variable
   (defaults to `0.0.0`).
4. **CI produces cross-platform artifacts:** GitHub Actions builds on macOS, Windows, and
   many Linux architectures via Docker+QEMU. Do not change compiler or Docker base images
   without checking matrix impact.
5. **Sensitive files:** Do not read or surface `.env`, `credentials.json`, or any file
   containing tokens/keys.
6. **`build/` is gitignored.** Generated artifacts and `compile_commands.json` live there;
   do not commit them.
7. **Use `project.py` for routine work.** It handles environment checks, version propagation,
   and test filtering consistently across platforms.

## AI do / don't

- DON'T round / drop astronomical constants or loosen tolerances to pass CI (see above).
- DON'T ASCII-ise unicode identifiers, move logic out of headers, or add namespace-scope
  `using` to a header.
- DON'T add a dependency or build step outside the `project.py` / `linter.py` flow.
- Match the neighbouring header's texture; internal consistency > external "best practice".

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
