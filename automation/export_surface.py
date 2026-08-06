# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# License: GNU General Public License v3.0
#
# This software is distributed without any warranty.
# See <https://www.gnu.org/licenses/> for more details.

import re
import shutil

from pathlib import Path
from typing import Dict, Final, List, Optional, Tuple

from . import paths
from .abi_layout import STRUCT_RE
from .utils import run_cmd, green_print, red_print, yellow_print


# Non-entry exports that must stay exported anyway, each mapped to the reason it cannot
# be hidden (#91: list every survivor with its reason, never fold them into a count).
# Empty today: every current survivor is a std vague-linkage weak/unique symbol, matched
# by the vague-linkage predicate below rather than listed here. The human-facing copy of
# this list lives in src/shared_lib/CMakeLists.txt; this constant is the machine-readable
# source.
SURVIVOR_EXCEPTIONS: Final[Dict[str, str]] = {
}

# What a survivor may look like without being listed in SURVIVOR_EXCEPTIONS: vague-linkage
# artifacts of C++ itself. Namespace std carries an explicit default-visibility attribute
# that overrides -fvisibility=hidden, and cross-DSO coalescing of these weak symbols needs
# them visible; RTTI for builtin or function types is anonymous. Anything else that
# escapes the hiding must be named in SURVIVOR_EXCEPTIONS with a reason -- widening this
# predicate instead defeats the gate.

# Predicate self-test: std-owned shapes must pass, user-owned shapes must not -- in
# particular a user symbol whose argument, return type, or conversion-operator target
# merely mentions std:: (that substring must not whitewash it), and user-type RTTI,
# which unlike builtin/function-type RTTI names a type a consumer can link against.
PREDICATE_SELF_TEST: Final[List[Tuple[str, bool]]] = [
  ("char* std::__add_grouping<char>(char*, char*, char)", True),
  ("vtable for std::vector<double, std::allocator<double> >", True),
  ("typeinfo for double", True),
  ("typeinfo for double (int, calendar::jieqi::Jieqi)", True),
  ("typeinfo name for std::vector<double>", True),
  ("astro::sun::geocentric_coord(double)", False),
  ("astro::foo(std::basic_string<char, std::char_traits<char> >)", False),
  ("std::vector<double> astro::make_vec(double)", False),
  ("astro::Holder<std::string>::get()", False),
  ("astro::Foo::operator std::vector<double>()", False),
  ("typeinfo for astro::Foo", False),
  ("vtable for astro::Foo", False),
  # The hole below, pinned on the shape that can actually reach the gate: a
  # global-namespace user type is spelled like a builtin, and celestial.h's C-ABI
  # structs are exactly that. Tightening the rule turns this line red instead of
  # silently changing what the gate accepts.
  ("typeinfo for JulianDay", True),
  # The other pass-through, for symmetry: "(" is a stand-in for "function type".
  ("typeinfo for astro::Fn<double (int)>", True),
]


def is_std_vague_linkage(demangled: str) -> bool:
  """Whether a demangled survivor is a C++ vague-linkage artifact rather than API surface."""
  # RTTI. The line to draw is whether the name mentions a type this library owns: that
  # is what a consumer could name, catch, or dynamic_cast against, so it takes the
  # qualified-name check like a vtable. Being a defined data symbol does not separate
  # anything -- function-type RTTI is one too (this .so exports `typeinfo for double
  # (int, calendar::jieqi::Jieqi)`).
  #
  # The name is all we have, and it leaves one hole: a global-namespace user type is
  # spelled exactly like a builtin. celestial.h's C-ABI structs are that shape; they
  # emit no RTTI today because they are non-polymorphic and never an operand of typeid
  # or throw -- POD-ness is not what decides it. Closing the hole means enumerating the
  # builtin spellings; until something leaks, the hole is pinned in the self-test above.
  # Using "(" for "function type" is the other approximation, kept for the same reason.
  for prefix in ("typeinfo for ", "typeinfo name for "):
    if demangled.startswith(prefix):
      rest = demangled[len(prefix):]
      if "(" in rest or "::" not in rest:
        return True
      return rest.startswith(("std::", "__gnu_cxx::"))
  # Only the qualified name may lend its std::-ness. Squeeze spaces inside <...> so the
  # name is a single token, drop the argument list (the first "(" outside any template),
  # and take the last token before it -- a return type, when demangled, comes first, and
  # prefixes of the "vtable for X" shape fall away the same way. A conversion operator
  # puts its target type AFTER the name, so cut at "::operator " first.
  squeezed: List[str] = []
  depth = 0
  for ch in demangled:
    if ch == "<":
      depth += 1
    elif ch == ">":
      depth -= 1
    if ch != " " or depth == 0:
      squeezed.append(ch)
  head = "".join(squeezed)
  depth = 0
  cut = len(head)
  for i, ch in enumerate(head):
    if ch == "<":
      depth += 1
    elif ch == ">":
      depth -= 1
    elif ch == "(" and depth == 0:
      cut = i
      break
  name = head[:cut].split("::operator ", 1)[0].rsplit(" ", 1)[-1]
  return name.startswith(("std::", "__gnu_cxx::"))


def self_test_predicate() -> List[str]:
  """Hold the vague-linkage predicate to pinned shapes; a failure means the gate is broken."""
  failures: List[str] = []
  for sample, expected in PREDICATE_SELF_TEST:
    actual = is_std_vague_linkage(sample)
    if actual != expected:
      failures.append(f"predicate self-test: {sample!r} -> {actual}, expected {expected}")
  return failures

# Parser self-test samples (shape -> expected (name, has_macro) pairs). The gate runs
# these before trusting the parser: a parser that cannot read the declaration shapes in
# use would report a maimed entry set and call it green. One sample per shape in play:
# macro-prefixed plain, pointer return type, multi-line parameter list, and one
# declaration without the macro (the mutation the static half exists to catch).
PARSER_SELF_TEST: Final[List[Tuple[str, List[Tuple[str, bool]]]]] = [
  ("CELESTIAL_API bool alpha(uint8_t v);", [("alpha", True)]),
  ("CELESTIAL_API const char *beta(void);", [("beta", True)]),
  ("CELESTIAL_API uint32_t gamma(int32_t year,\n"
   "                                double longitude,\n"
   "                                double *slots,\n"
   "                                uint32_t slot_count);", [("gamma", True)]),
  ("double delta(double jde);", [("delta", False)]),
]

FUNC_NAME_RE: Final[re.Pattern] = re.compile(r"(\w+)\s*\(")
SONAME_RE: Final[re.Pattern] = re.compile(r"Library soname: \[([^\]]+)\]")
REAL_SO_RE: Final[re.Pattern] = re.compile(r"^libcelestial_calendar\.so\.(\d+)\.(\d+)\.(\d+)$")


def strip_comments(text: str) -> str:
  """Remove /* */ and // comments so they cannot fake or hide declarations."""
  text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
  return re.sub(r"//[^\n]*", "", text)


def parse_declarations(text: str) -> List[Tuple[str, bool]]:
  """Parse file-scope function declarations in `celestial.h` -> (name, has_macro) pairs.

  Preprocessor lines and `typedef struct` bodies are removed first; what remains at
  file scope is split into `;`-terminated statements, and every statement containing
  `(` is a function declaration. Declarations WITHOUT the macro are returned too --
  flagging them is exactly what the static half of the gate is for.
  """
  text = strip_comments(text)
  text = "\n".join(line for line in text.splitlines() if not line.lstrip().startswith("#"))
  text = STRUCT_RE.sub("", text)

  decls: List[Tuple[str, bool]] = []
  for stmt in text.split(";"):
    if "(" not in stmt:
      continue
    m = FUNC_NAME_RE.search(stmt)
    if m is None:
      continue
    decls.append((m.group(1), "CELESTIAL_API" in stmt))
  return decls


def entry_point_names(header: Path) -> List[str]:
  """The entry-point set of `celestial.h`, macro status ignored (the gate checks that)."""
  return sorted(name for name, _ in parse_declarations(header.read_text()))


def self_test_parser() -> List[str]:
  """Hold the parser to the pinned shape samples; a failure means the gate is broken."""
  failures: List[str] = []
  for sample, expected in PARSER_SELF_TEST:
    actual = parse_declarations(sample)
    if actual != expected:
      failures.append(f"parser self-test: {sample!r} -> {actual!r}, expected {expected!r}")
  return failures


def find_real_so(shared_lib_dir: Path) -> Optional[Path]:
  """Locate the real built library (not a symlink); None means the build is missing."""
  for candidate in sorted(shared_lib_dir.iterdir()) if shared_lib_dir.is_dir() else []:
    if REAL_SO_RE.match(candidate.name) and not candidate.is_symlink():
      return candidate
  return None


def nm_defined(so: Path) -> Dict[str, str]:
  """`nm -D --defined-only` as a name -> type-letter map. A missing nm turns the gate red."""
  if shutil.which("nm") is None:
    raise RuntimeError("nm not found (needs binutils)")
  ret = run_cmd(["nm", "-D", "--defined-only", str(so)], print_cmd=False, print_stdout=False)
  if ret.retcode != 0:
    raise RuntimeError(f"nm failed on {so} (exit {ret.retcode})")
  symbols: Dict[str, str] = {}
  for line in ret.stdout.splitlines():
    parts = line.split()
    if len(parts) >= 3:
      symbols[parts[2]] = parts[1]
  return symbols


def demangle(names: List[str]) -> Dict[str, str]:
  """c++filt the given names. A missing c++filt turns the gate red, never a silent pass."""
  if shutil.which("c++filt") is None:
    raise RuntimeError("c++filt not found (needs binutils)")
  ret = run_cmd(["c++filt"] + names, print_cmd=False, print_stdout=False)
  if ret.retcode != 0:
    raise RuntimeError(f"c++filt failed (exit {ret.retcode})")
  return dict(zip(names, ret.stdout.splitlines(), strict=True))


def read_soname(so: Path) -> Optional[str]:
  """The DT_SONAME of the built library, via `readelf -d`."""
  if shutil.which("readelf") is None:
    raise RuntimeError("readelf not found (needs binutils)")
  ret = run_cmd(["readelf", "-d", str(so)], print_cmd=False, print_stdout=False)
  if ret.retcode != 0:
    raise RuntimeError(f"readelf failed on {so} (exit {ret.retcode})")
  m = SONAME_RE.search(ret.stdout)
  return m.group(1) if m else None


def check_export_surface() -> int:
  """Hold the export surface of the built library to the entry points of `celestial.h` (#91).

  Two halves, each covering what the other cannot:
  - static: every file-scope declaration in celestial.h carries CELESTIAL_API. A missing
    macro is a missing export on Windows -- caught here, without needing a Windows leg.
  - dynamic: the strong defined symbols of the built .so are exactly the entry set plus
    SURVIVOR_EXCEPTIONS, and every survivor beyond that is a std vague-linkage weak/unique
    symbol (the vague-linkage predicate). This is the half that proves the hiding held.
  Plus a SONAME pin: DT_SONAME must be the major.minor form of the real file's version,
  which is the promise the CMakeLists SOVERSION comment makes while 0.x.

  Needs the built library (`./project.py --build`): a missing build turns the gate red,
  never a silent skip. The parser self-test runs first -- a broken parser must fail the
  gate, not pass it with a maimed entry set.
  """
  print("#" * 60)
  yellow_print("Checking that the export surface matches the celestial.h entry points...")

  failures: List[str] = self_test_parser() + self_test_predicate()
  if failures:
    for f in failures:
      red_print(f"  - {f}")
    red_print("Export-surface gate is broken (self-test failed); fix the gate, not the data")
    return 1

  header = paths.proj_root() / "src" / "shared_lib" / "celestial.h"
  decls = parse_declarations(header.read_text())
  if not decls:
    red_print(f"No entry declarations parsed from {header}; the parser is broken")
    return 1

  # Static half: every declaration carries the macro.
  for name, has_macro in decls:
    if not has_macro:
      failures.append(f"{name}: declaration in celestial.h without CELESTIAL_API")
  entries = {name for name, _ in decls}

  # Dynamic half, on the real built library. A missing build or a missing/failing
  # binutils tool fails the gate but must not swallow the findings collected so far,
  # static or dynamic -- the static half is the part any leg can check.
  so = find_real_so(paths.build_dir() / "shared_lib")
  soname: Optional[str] = None
  if so is None:
    failures.append("Built library not found (run ./project.py --build first); dynamic half not run")
  else:
    try:
      symbols = nm_defined(so)
      exported = set(symbols.keys())

      missing = sorted(entries - exported)
      for name in missing:
        failures.append(f"{name}: entry point not exported by {so.name}")

      survivors = sorted(exported - entries)
      demangled = demangle(survivors) if survivors else {}
      for name in survivors:
        if name in SURVIVOR_EXCEPTIONS:
          continue
        if is_std_vague_linkage(demangled[name]):
          continue
        failures.append(
          f"{name} ({demangled[name]}): exported but neither an entry point nor a std "
          f"vague-linkage symbol -- hide it, or add it to SURVIVOR_EXCEPTIONS with a reason"
        )

      # SONAME pin: major.minor of the real file's version, per the CMakeLists comment.
      version = REAL_SO_RE.match(so.name)
      assert version is not None  # find_real_so only returns REAL_SO_RE matches
      expected_soname = f"libcelestial_calendar.so.{version.group(1)}.{version.group(2)}"
      soname = read_soname(so)
      if soname != expected_soname:
        failures.append(f"SONAME is {soname!r}, expected {expected_soname!r} (major.minor of VERSION while 0.x)")
    except Exception as e:  # noqa: BLE001 -- see below
      # Deliberately broad: the sentence above promises that no tool failure loses the
      # static findings, and the failure modes are not all `RuntimeError` -- c++filt can
      # exit 0 with a short output (`ValueError` from the strict `zip`), and an exec that
      # gets past `which` raises `OSError`. Narrowing this re-opens the swallow.
      failures.append(f"{type(e).__name__}: {e}")

  print("#" * 60)
  if failures:
    red_print(f"Export-surface gate failed ({len(failures)} finding(s)):")
    for f in failures:
      red_print(f"  - {f}")
    return 1

  green_print(f"export surface == celestial.h entry points ({len(entries)} entries, "
              f"{len(survivors)} std vague-linkage survivors, SONAME {soname})")
  return 0
