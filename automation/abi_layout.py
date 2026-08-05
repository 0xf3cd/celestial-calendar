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

import ast
import ctypes
import os
import re
import shutil
import tempfile

from pathlib import Path
from typing import Dict, List, Final, Optional, Tuple

from . import paths
from .utils import run_cmd, green_print, red_print, yellow_print


# The closed set of field types the gate understands, mapped to their ctypes mirror.
# A field type outside it fails the gate loudly: extending the ABI with a new field
# type is a deliberate act that must extend this table, not something to wave through.
C_TO_CTYPES: Final[Dict[str, str]] = {
  "bool":     "c_bool",
  "uint8_t":  "c_uint8",
  "uint16_t": "c_uint16",
  "uint32_t": "c_uint32",
  "int32_t":  "c_int32",
  "double":   "c_double",
}

STRUCT_RE: Final[re.Pattern] = re.compile(r"typedef\s+struct\s+(\w+)\s*\{(.*?)\}\s*\1\s*;", re.DOTALL)
FIELD_RE: Final[re.Pattern] = re.compile(r"^\s*(\w+)\s+(\w+)\s*;")

# (field name, type name), in declaration order.
Fields = List[Tuple[str, str]]


def parse_c_structs(header: Path) -> Dict[str, Fields]:
  """Parse every `typedef struct` in `celestial.h`: name -> fields in declaration order."""
  structs: Dict[str, Fields] = {}
  for name, body in STRUCT_RE.findall(header.read_text()):
    fields = [(m.group(2), m.group(1)) for line in body.splitlines() if (m := FIELD_RE.match(line))]
    if not fields:
      raise RuntimeError(f"No fields parsed from struct {name} in {header}")
    structs[name] = fields
  if not structs:
    raise RuntimeError(f"No typedef struct found in {header}")
  return structs


def parse_py_structs(mirror: Path) -> Dict[str, Fields]:
  """Parse every ctypes `Structure` subclass in `common.py`, statically (no import, no .so).

  Class names are normalized by stripping one leading underscore (`_JulianDay` mirrors
  `JulianDay`; `DeltaT` carries no underscore). Anything the parser cannot read exactly --
  a missing `_fields_`, a non-tuple entry, a non-simple type name -- raises, because a
  gate that guesses is no gate.
  """
  structs: Dict[str, Fields] = {}
  for node in ast.parse(mirror.read_text()).body:
    if not isinstance(node, ast.ClassDef):
      continue
    is_structure = any(
      (isinstance(b, ast.Name) and b.id == "Structure") or
      (isinstance(b, ast.Attribute) and b.attr == "Structure")
      for b in node.bases
    )
    if not is_structure:
      continue

    fields: Optional[Fields] = None
    for stmt in node.body:
      if not (isinstance(stmt, ast.Assign) and
              any(isinstance(t, ast.Name) and t.id == "_fields_" for t in stmt.targets)):
        continue
      if not isinstance(stmt.value, (ast.List, ast.Tuple)):
        raise RuntimeError(f"Unreadable _fields_ on Structure subclass {node.name}: not a literal list/tuple")
      fields = []
      for elt in stmt.value.elts:
        if not isinstance(elt, ast.Tuple) or len(elt.elts) != 2:
          raise RuntimeError(f"Unreadable _fields_ entry in {node.name}: not a (name, type) 2-tuple")
        fname_node, type_node = elt.elts
        if not isinstance(fname_node, ast.Constant) or not isinstance(fname_node.value, str):
          raise RuntimeError(f"Unreadable field name in {node.name}._fields_")
        if isinstance(type_node, ast.Name):
          tname = type_node.id
        elif isinstance(type_node, ast.Attribute):
          tname = type_node.attr
        else:
          raise RuntimeError(f"Unreadable field type in {node.name}._fields_")
        fields.append((fname_node.value, tname))

    if not fields:
      raise RuntimeError(f"No readable _fields_ on Structure subclass {node.name}")
    structs[node.name[1:] if node.name.startswith("_") else node.name] = fields

  if not structs:
    raise RuntimeError(f"No Structure subclass found in {mirror}")
  return structs


def c_layout(structs: Dict[str, Fields], header: Path, workdir: Path) -> Optional[Dict[str, int]]:
  """Compile and run a tiny C program printing sizeof/offsetof for every struct and field.

  This is the ground-truth side of the comparison: the values the C ABI actually has.
  A missing or failing compiler must turn the gate red, never silently skip it.
  """
  cc = os.environ.get("CC", "cc")
  if shutil.which(cc) is None:
    red_print(f"C compiler not found: {cc} (set $CC)")
    return None

  lines = ['#include "celestial.h"', "#include <stdio.h>", "#include <stddef.h>", "int main(void) {"]
  for name, fields in structs.items():
    lines.append(f'  printf("{name} %zu\\n", sizeof({name}));')
    for fname, _ in fields:
      lines.append(f'  printf("{name}.{fname} %zu\\n", offsetof({name}, {fname}));')
  lines += ["  return 0;", "}"]

  src = workdir / "abi_layout_probe.c"
  exe = workdir / "abi_layout_probe"
  src.write_text("\n".join(lines) + "\n")

  ret = run_cmd([cc, "-Wall", "-Werror", f"-I{header.parent}", str(src), "-o", str(exe)],
                print_cmd=False, print_stdout=False, print_stderr=False)
  if ret.retcode != 0:
    red_print(f"Layout probe failed to compile with {cc}:")
    for line in (ret.stderr or ret.stdout).splitlines()[:12]:
      print(f"  {line}")
    return None

  ret = run_cmd([str(exe)], print_cmd=False, print_stdout=False, print_stderr=False)
  if ret.retcode != 0:
    red_print(f"Layout probe failed to run (exit {ret.retcode})")
    return None

  layout: Dict[str, int] = {}
  for line in ret.stdout.splitlines():
    key, _, value = line.rpartition(" ")
    layout[key] = int(value)
  return layout


def py_layout(fields: Fields) -> Tuple[int, Dict[str, int]]:
  """Build the mirrored struct with ctypes (stdlib only, no .so) and read its layout."""
  ctype_fields = [(fname, getattr(ctypes, tname)) for fname, tname in fields]
  cls = type("_Probe", (ctypes.Structure,), {"_fields_": ctype_fields})
  return ctypes.sizeof(cls), {fname: getattr(cls, fname).offset for fname, _ in ctype_fields}


def check_abi_layout() -> int:
  """Hold the `statistics/common.py` ctypes mirror to the real layout in `celestial.h`.

  Every struct in `celestial.h` has a ctypes mirror in `common.py`, and a struct read back
  through a drifted mirror is garbage no test prints (#85). Three layers, because
  each sees what the others cannot: set equality catches a missing or extra mirror,
  sizeof/offsetof catches layout drift, and the type-name map catches what raw layout
  cannot -- signedness and same-width swaps (c_int32 vs c_uint32, c_bool vs c_uint8).
  """
  print("#" * 60)
  yellow_print("Checking that the ctypes mirror matches the real ABI layout...")

  header = paths.proj_root() / "src" / "shared_lib" / "celestial.h"
  mirror = paths.proj_root() / "statistics" / "common.py"

  try:
    c_structs = parse_c_structs(header)
    py_structs = parse_py_structs(mirror)
  except RuntimeError as e:
    red_print(f"Parse failed: {e}")
    return 1

  failures: List[str] = []

  # Layer 1: set equality, both directions.
  for name in sorted(c_structs.keys() - py_structs.keys()):
    failures.append(f"{name}: no ctypes mirror in common.py")
  for name in sorted(py_structs.keys() - c_structs.keys()):
    failures.append(f"{name}: ctypes mirror without a struct in celestial.h")

  # Layer 2: compile and run the ground-truth probe once for every struct.
  with tempfile.TemporaryDirectory(prefix="abi_layout_") as tmp:
    ground_truth = c_layout(c_structs, header, Path(tmp))
  if ground_truth is None:
    return 1

  # Layers 2+3: per-struct layout and type-name comparison.
  for name in sorted(c_structs.keys() & py_structs.keys()):
    c_fields = c_structs[name]
    py_fields = py_structs[name]
    py_size, py_offsets = py_layout(py_fields)

    if ground_truth[name] != py_size:
      failures.append(f"{name}: sizeof {ground_truth[name]} (C) != {py_size} (ctypes)")

    c_names = [f for f, _ in c_fields]
    py_names = [f for f, _ in py_fields]
    for fname in sorted(set(c_names) - set(py_names)):
      failures.append(f"{name}.{fname}: field missing from the ctypes mirror")
    for fname in sorted(set(py_names) - set(c_names)):
      failures.append(f"{name}.{fname}: field without a counterpart in celestial.h")

    py_type_of = dict(py_fields)
    for fname, ctype in c_fields:
      if ctype not in C_TO_CTYPES:
        failures.append(f"{name}.{fname}: C type {ctype} is outside the gate's closed set -- extend C_TO_CTYPES")
        continue
      if fname not in py_type_of:
        continue  # already reported above
      expected = C_TO_CTYPES[ctype]
      if py_type_of[fname] != expected:
        failures.append(f"{name}.{fname}: {ctype} mirrored as {py_type_of[fname]}, expected {expected}")
      if py_offsets[fname] != ground_truth[f"{name}.{fname}"]:
        failures.append(
          f"{name}.{fname}: offset {ground_truth[f'{name}.{fname}']} (C) != {py_offsets[fname]} (ctypes)"
        )

  print("#" * 60)
  if failures:
    red_print(f"ABI layout gate failed ({len(failures)} finding(s)):")
    for f in failures:
      red_print(f"  - {f}")
    return 1

  green_print(f"ctypes mirror matches the ABI layout ({len(c_structs)} structs)")
  return 0
