# Building CelestialCalendar

<a id="requirements"></a>
## Requirements

These are source-development instructions, not prerequisites for using installed packages. Installed packages
need only their own [Python](../README.md#python), [JavaScript](../README.md#javascript) or
[native runtime](../README.md#native-runtime) requirements. Building the core library from source needs:

- A C++23 compiler. CI uses clang++ 22 on Linux and Windows, Xcode 26's Apple clang on macOS, and g++ 14 on
  Linux; older compilers may work but are not checked.
- CMake ≥ 3.22 and make.
- Python 3 for build and test automation; its dependencies are in `Requirements.txt`.

Debian, Ubuntu and other distributions protect the system Python under PEP 668, so use a virtual environment as
shown in [Building from Source](#build). `--setup` installs nothing once the dependencies are present.
`Requirements.txt` does not include the [static-check tools](#lint); the notebooks and data-collection scripts in
`statistics/` use `Requirements-statistics.txt` instead.

<a id="build"></a>
## Building from Source

This section is for users and contributors who need to build the core library. Run every command from the root
of a full source checkout; outputs go to `build/`. [`AGENTS.md`](../AGENTS.md) is the source of truth for build,
test, code-style and contribution conventions, including direct test-binary execution and reconciliation against
the test macros rather than accepting ctest counts alone.

Python wheels have their own [local wheel recipe](../bindings/python/README.md#build-a-local-wheel); do not build
from a copy of the `bindings/python/` directory alone. The recipe uses the full checkout, hash-locked Python build
requirements, system CMake, and an installed C/C++23 toolchain. It builds only a wheel, not an sdist. That
host-local wheel uses the host's native libraries; it is not an official repaired portable wheel and does not
extend the official platform list.

### macOS / Linux

With a C++23 compiler, CMake, make and Python 3 installed, create and activate a virtual environment, then run the
single entry point:

```sh
python3 -m venv .venv
. .venv/bin/activate
export CXX=clang++
python project.py --all
```

`--all` runs setup, configure, build and test in that order. You can also run the steps separately or pick
individual tasks:

```sh
python project.py --setup
python project.py --cmake --build
python project.py --test
CELESTIAL_TEST_SEED=123 python project.py --test
python project.py --bench
python project.py --help
```

Randomized tests default to seed 42; set `CELESTIAL_TEST_SEED` to replay a failure or explore other seeds.
Benchmarks are optional and not part of `--all`. `python project.py --clean` removes build outputs.

### Windows

Besides Python 3 and CMake, Windows needs LLVM and make, because the build uses CMake's `Unix Makefiles`
generator. With Chocolatey installed, `choco install -y make llvm` supplies those two tools. From the source root
in PowerShell, set both the LLVM C and C++ compilers so the toolchains are not mixed:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
$env:CXX = "clang++"
$env:CC = "clang"
python project.py --all
```

The step-by-step commands are the same as above; set the random seed with `$env:CELESTIAL_TEST_SEED = "123"`.

<a id="api-docs"></a>
### API Documentation

Install Doxygen 1.15.0, run `python project.py --docs` from the source root, and open
`build/api-docs/html/index.html`. This task does not compile C++, does not need Graphviz, and is not part of
`--all`. [Core Tests](https://github.com/0xf3cd/celestial-calendar/actions/workflows/core_tests.yml) uploads the
same HTML as the `celestial-api-html` attachment; [`API.md`](API.md) describes it. The interface is English; the
lunar subtree keeps its existing English/Chinese comments. The HTML is a CI attachment, not a hosted site or
release artifact.

<a id="wasm"></a>
## WebAssembly and npm Packages

Using the published npm packages does not need the tools in this section. It covers source builds and artifact
maintenance, and needs Node ≥ 22, the Python build dependencies and an emsdk checkout. Point to emsdk with the
`EMSDK` environment variable or `build_wasm.py --emsdk`. From the root of a full source checkout:

```sh
npm ci --ignore-scripts --prefix bindings/javascript
python3 toolbox/build_wasm.py
node toolbox/wasm_check.mjs
python3 toolbox/build_npm.py
```

Ordinary `npm pack` in the `bindings/javascript` development directory refuses to pack; use the builder above.

The generated module is `build/wasm/celestial-jieqi.mjs` with `build/wasm/celestial-jieqi.wasm`, and it carries
all 30 stable exports of `celestial.h`. `@0xf3cd/celestial` provides the existing API through the `config`,
`time`, `sun`, `moon`, `jieqi` and `lunar` namespaces; the new `chart_snapshot_v1` exists only in the private
WASM/ABI layer, and there is no JavaScript snapshot entry yet. Heap pointers, the count/fill protocol, sret layout
and `last_error` stay inside the package.

`build_npm.py` uses the generated module and the version from `project.py` to pack
an npm primary package of exactly 12 files. `@0xf3cd/celestial/date` imports only pure validation functions, not
the root entry or the Emscripten glue. The same builder also packs `celestial-calendar`,
an npm alias package of exactly 7 files that forwards the ESM and type declarations of the root entry and `/date`.
The alias's only runtime dependency is `@0xf3cd/celestial` at exactly the same version; it carries no copy of the
WASM. The two names share export references and initialization state only when both resolve to the same
installation of the primary package; this is not a singleton guarantee across arbitrary mixed-version dependency
graphs.

The builder packs the primary and alias packages once each. `build/npm/npm-pack.json` and
`build/npm/npm-alias-pack.json` select the respective tarballs; do not select them by glob or rebuild them for each
consumer.

The alias's MIT `LICENSE` covers its own material;
retained-component notices remain in the primary dependency.

The separate `wasm.yml` CI workflow builds the module and both packages. The `celestial-wasm` artifact contains
exactly 10 top-level files: `celestial-jieqi.mjs`, `celestial-jieqi.wasm`, `LICENSE`, `THIRD_PARTY_NOTICES.txt`,
the npm primary package tarball exactly as packed, `npm-pack.json`, `npm-pack.sha256`, and the alias package
tarball, `npm-alias-pack.json`, `npm-alias-pack.sha256`. Each metadata file names its own tarball. The release
workflow publishes the primary package first and the alias second, without rebuilding or repacking. Historical
0.6.x archives keep the original single-package format.

The same workflow checks 30 signatures and 19 layouts and replays the 389-point golden data generated natively.
It installs the same pair of packages into applications outside the source directory on the minimum supported and
the current Node version, compiles the installed type declarations for both package names and `/date`, and runs
Astro / Vite production-build smoke tests in Chrome and Firefox. Windows/macOS current-Node consumers download the
original pair from the same CI run and check installation, API calls, alias identity and the separate date-only
entry, without another build or pack.

<a id="lint"></a>
## Static Checks

C++ uses `clang-tidy` and Python uses Ruff, both run through `checks.py`; neither is in `Requirements.txt`.
Install them separately in the virtual environment used for the build:

```sh
python -m pip install ruff
python -m pip install clang-tidy==22.1.8
```

CI uses the clang-tidy 22.1.2 bundled with the runner, a patch release that pip does not provide; locally,
22.1.x or the distribution's `clang-tidy-22` works. The configuration is in `.clang-tidy` and enables
`WarningsAsErrors: '*'`. If the major version on `PATH` differs, set `CLANG_TIDY` to the executable you need.
It must match the repository's `run-clang-tidy.py`; otherwise the checks differ from CI's.

Run from the source root; these commands also work on Windows:

```sh
python checks.py --ruff
python checks.py --clang-tidy
```

<a id="artifacts"></a>
## CI Artifact Downloads

This section is for contributors and for maintainers who run CI. Users should take the
[public release downloads](#releases), which need no token. In GitHub, open the native,
[WASM](https://github.com/0xf3cd/celestial-calendar/actions/workflows/wasm.yml) or
[Python wheel](https://github.com/0xf3cd/celestial-calendar/actions/workflows/python-wheel.yml) workflow you need,
and download the artifacts from a completed run that built the target commit.

For automated downloads, use `toolbox/artifact_downloader.py`. From the source root, in an environment with the
`Requirements.txt` dependencies installed, supply GitHub authentication through the `GITHUB_TOKEN` environment
variable and run:

```sh
python3 toolbox/artifact_downloader.py -s build/artifacts --unzip
python3 toolbox/artifact_downloader.py --help
```

By default it downloads the complete native, WASM and Python artifacts built from HEAD. `-id` selects a run ID and
`-s` the destination directory; without `--unzip` the archives stay zipped. `--help` documents the remaining
options. The tool can also be called as `python3 -m toolbox.artifact_downloader`.

<a id="releases"></a>
## Release Downloads

Users open [Releases](https://github.com/0xf3cd/celestial-calendar/releases) directly, choose a version, and
download the native / WASM archives, Python wheels with SHA-256 sidecars, or source archives. Web downloads need
no token. A release may differ from the source version described here, so use the documentation that ships with
the selected release; building this checkout does not publish a version.

Maintainers who need scripted bulk downloads can use `toolbox/release_downloader.py`. This optional tool uses
GitHub authentication and is not needed for public downloads. Run it from the source root in an environment with
the `Requirements.txt` dependencies installed and `GITHUB_TOKEN` set:

```sh
python3 toolbox/release_downloader.py -s build/releases
python3 toolbox/release_downloader.py --help
```

By default it downloads the files of the latest release, and `-s` selects the destination directory; `--help`
documents the remaining options. The tool can also be called as `python3 -m toolbox.release_downloader`. For CI
artifacts, see [CI Artifact Downloads](#artifacts). Maintainers publishing a new release must follow
[`docs/RELEASING.md`](RELEASING.md), not the consumer install instructions; a successful download or build does
not complete a release.

<a id="todo"></a>
## Toolchain Feature Tracking

The code waits for full toolchain support before it adopts:

- modules;
- ranges extensions such as `std::views::enumerate` / `pairwise`;
- `std::generator` in the moon_phase / jieqi Newton iterations.

`./checks.py --features` checks these features by actually compiling them. CI runs it against libstdc++, libc++
and the MSVC STL separately, and reports when a toolchain already supports a feature while the code still keeps
its hand-written replacement, so this list does not go stale.
