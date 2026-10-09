# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import ast
import ctypes
import os
import re
import shutil
import tempfile

from pathlib import Path
from typing import Dict, List, Optional, Tuple

from . import paths
from .c_abi_schema import C_TO_CTYPES, Fields, parse_c_structs, split_field_type
from .utils import run_cmd, green_print, red_print, yellow_print


def mirror_type(node: ast.expr, records: dict[str, str], owner: str) -> str:
  """Read a ctypes scalar, earlier record, or literal bounded array as a C type."""
  if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Mult):
    if not isinstance(node.right, ast.Constant) or type(node.right.value) is not int or node.right.value <= 0:
      raise RuntimeError(f"Unsupported array extent in {owner}")
    base = mirror_type(node.left, records, owner)
    if split_field_type(base)[1] is not None:
      raise RuntimeError(f"Multidimensional array in {owner}")
    return f"{base}[{node.right.value}]"

  if isinstance(node, ast.Name):
    name = node.id
    if name in records:
      return records[name]
  elif isinstance(node, ast.Attribute) and isinstance(node.value, ast.Name) and node.value.id == "ctypes":
    name = node.attr
  else:
    raise RuntimeError(f"Unreadable field type in {owner}")
  primitives = {python: c for c, python in C_TO_CTYPES.items()}
  if name not in primitives:
    raise RuntimeError(f"Unknown or not previously declared field type in {owner}: {name}")
  return primitives[name]


def _structure_name(node: ast.ClassDef) -> str | None:
  is_structure = any(
    (isinstance(base, ast.Name) and base.id == "Structure")
    or (isinstance(base, ast.Attribute) and base.attr == "Structure")
    for base in node.bases
  )
  if not is_structure:
    return None

  if node.decorator_list or node.keywords:
    raise RuntimeError(f"Computed Structure declaration in {node.name}")
  if len(node.bases) != 1 or not (
    isinstance(node.bases[0], ast.Name)
    and node.bases[0].id == "Structure"
    or isinstance(node.bases[0], ast.Attribute)
    and isinstance(node.bases[0].value, ast.Name)
    and node.bases[0].value.id == "ctypes"
    and node.bases[0].attr == "Structure"
  ):
    raise RuntimeError(f"Unsupported Structure base in {node.name}")

  return node.name[1:] if node.name.startswith("_") else node.name


def _mirror_fields(node: ast.ClassDef, records: dict[str, str]) -> Fields:
  for child in ast.walk(node):
    if isinstance(child, (ast.Delete, ast.NamedExpr)):
      raise RuntimeError(f"Computed or deleted field declaration in {node.name}")
    if isinstance(child, (ast.Assign, ast.AnnAssign, ast.AugAssign)) and child not in node.body:
      targets = child.targets if isinstance(child, ast.Assign) else [child.target]
      if any(isinstance(target, ast.Name) and target.id == "_fields_" for target in targets):
        raise RuntimeError(f"Conditional or computed _fields_ in {node.name}")
    if isinstance(child, ast.Call) and isinstance(child.func, ast.Attribute):
      owner = child.func.value
      if isinstance(owner, ast.Name) and owner.id == "_fields_":
        raise RuntimeError(f"Computed _fields_ mutation in {node.name}")

  fields: Optional[Fields] = None
  for stmt in node.body:
    if isinstance(stmt, (ast.Assign, ast.AnnAssign, ast.AugAssign)):
      targets = stmt.targets if isinstance(stmt, ast.Assign) else [stmt.target]
      if any(
        isinstance(target, ast.Name) and target.id in {"_pack_", "_align_", "_layout_", "_anonymous_"}
        for target in targets
      ):
        raise RuntimeError(f"Unsupported layout override in {node.name}")
      if not isinstance(stmt, ast.Assign) and any(
        isinstance(target, ast.Name) and target.id == "_fields_" for target in targets
      ):
        raise RuntimeError(f"Nonliteral _fields_ assignment in {node.name}")

    if not (
      isinstance(stmt, ast.Assign)
      and any(isinstance(target, ast.Name) and target.id == "_fields_" for target in stmt.targets)
    ):
      continue
    if fields is not None or len(stmt.targets) != 1:
      raise RuntimeError(f"Duplicate or chained _fields_ assignment in {node.name}")
    if not isinstance(stmt.value, (ast.List, ast.Tuple)):
      raise RuntimeError(f"Unreadable _fields_ on Structure subclass {node.name}: not a literal list/tuple")

    fields = []
    for elt in stmt.value.elts:
      if not isinstance(elt, ast.Tuple) or len(elt.elts) != 2:
        raise RuntimeError(f"Unreadable _fields_ entry in {node.name}: not a (name, type) 2-tuple")
      fname_node, type_node = elt.elts
      if not isinstance(fname_node, ast.Constant) or not isinstance(fname_node.value, str):
        raise RuntimeError(f"Unreadable field name in {node.name}._fields_")
      if not re.fullmatch(r"[A-Za-z_]\w*", fname_node.value) or fname_node.value in dict(fields):
        raise RuntimeError(f"Invalid or duplicate field name in {node.name}: {fname_node.value!r}")
      tname = mirror_type(type_node, records, f"{node.name}.{fname_node.value}")
      fields.append((fname_node.value, tname))

  if not fields:
    raise RuntimeError(f"No readable _fields_ on Structure subclass {node.name}")
  return fields


def parse_py_structs(mirror: Path) -> Dict[str, Fields]:
  """Parse every ctypes `Structure` subclass in `common.py`, statically (no import, no .so).

  Class names are normalized by stripping one leading underscore (`_JulianDay` mirrors
  `JulianDay`; `DeltaT` carries no underscore). Fields use the same canonical C strings
  as the header. Unknown syntax, types and
  extents raise rather than being omitted from the comparison.
  """
  module = ast.parse(mirror.read_text(encoding="utf-8"))
  structs: Dict[str, Fields] = {}
  records: dict[str, str] = {}
  for node in ast.walk(module):
    if not isinstance(node, ast.ClassDef):
      continue
    name = _structure_name(node)
    if name is None:
      continue
    if name in structs or name in C_TO_CTYPES or node.name in C_TO_CTYPES.values():
      raise RuntimeError(f"Duplicate or reserved mirror name: {node.name}")
    fields = _mirror_fields(node, records)
    structs[name] = fields
    records[node.name] = name

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
    lines.append(f'  printf("{name}.alignment %zu\\n", _Alignof({name}));')
    for fname, field_type in fields:
      base, extent = split_field_type(field_type)
      lines.append(f'  printf("{name}.{fname} %zu\\n", offsetof({name}, {fname}));')
      lines.append(f'  printf("{name}.{fname}.size %zu\\n", sizeof((({name} *)0)->{fname}));')
      lines.append(f'  printf("{name}.{fname}.alignment %zu\\n", _Alignof({base}));')
      if extent is not None:
        lines.append(f'  printf("{name}.{fname}.stride %zu\\n", sizeof((({name} *)0)->{fname}[0]));')
        lines.append(
          f'  printf("{name}.{fname}.extent %zu\\n", '
          f"sizeof((({name} *)0)->{fname}) / sizeof((({name} *)0)->{fname}[0]));"
        )
  lines += ["  return 0;", "}"]

  src = workdir / "abi_layout_probe.c"
  exe = workdir / "abi_layout_probe"
  src.write_text("\n".join(lines) + "\n")

  ret = run_cmd(
    [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", f"-I{header.parent}", str(src), "-o", str(exe)],
    print_cmd=False,
    print_stdout=False,
    print_stderr=False,
  )
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


def py_layout(structs: Dict[str, Fields]) -> Dict[str, int]:
  """Measure ctypes records in declaration order, including nested and array storage."""
  types = {c: getattr(ctypes, python) for c, python in C_TO_CTYPES.items()}
  layout: Dict[str, int] = {}
  for name, fields in structs.items():
    ctype_fields = []
    for fname, field_type in fields:
      base, extent = split_field_type(field_type)
      element = types[base]
      field = element if extent is None else element * extent
      ctype_fields.append((fname, field))
      layout[f"{name}.{fname}.size"] = ctypes.sizeof(field)
      layout[f"{name}.{fname}.alignment"] = ctypes.alignment(field)
      if extent is not None:
        layout[f"{name}.{fname}.extent"] = field._length_
        layout[f"{name}.{fname}.stride"] = ctypes.sizeof(element)

    cls = type(name, (ctypes.Structure,), {"_fields_": ctype_fields})
    types[name] = cls
    layout[name] = ctypes.sizeof(cls)
    layout[f"{name}.alignment"] = ctypes.alignment(cls)
    layout.update({f"{name}.{fname}": getattr(cls, fname).offset for fname, _ in fields})
  return layout


def runtime_layout(module: object) -> Tuple[Dict[str, Fields], Dict[str, int]]:
  records: Dict[str, type] = {}
  for binding, value in vars(module).items():
    if not isinstance(value, type) or not issubclass(value, ctypes.Structure) or value is ctypes.Structure:
      continue
    name = binding[1:] if binding.startswith("_") else binding
    if name in records:
      raise RuntimeError(f"Multiple runtime bindings normalize to {name}")
    records[name] = value

  names = {value: name for name, value in records.items()}
  if len(names) != len(records):
    raise RuntimeError("Multiple mirror names bind the same runtime record")
  primitives = {getattr(ctypes, python): c for c, python in C_TO_CTYPES.items()}

  library = getattr(module, "LIB", None)
  if library is not None:
    for function in vars(library).values():
      restype = getattr(function, "restype", None)
      if isinstance(restype, type) and issubclass(restype, ctypes.Structure) and restype not in names:
        raise RuntimeError("Unmeasured runtime return record")

  def field_type(ctype: type) -> str:
    if issubclass(ctype, ctypes.Array):
      if ctype._length_ <= 0:
        raise RuntimeError("Empty runtime ABI array")
      return f"{field_type(ctype._type_)}[{ctype._length_}]"
    if ctype in names:
      return names[ctype]
    if ctype not in primitives:
      raise RuntimeError(f"Unsupported runtime ABI field type: {ctype}")
    return primitives[ctype]

  structs: Dict[str, Fields] = {}
  measured: Dict[str, int] = {}
  for name, record in records.items():
    actual_name = record.__name__[1:] if record.__name__.startswith("_") else record.__name__
    if actual_name != name:
      raise RuntimeError(f"Runtime record identity differs: {name} binds {actual_name}")
    fields = record._fields_
    if any(len(field) != 2 for field in fields):
      raise RuntimeError(f"Unsupported runtime fields in {name}")
    structs[name] = [(field_name, field_type(ctype)) for field_name, ctype in fields]

    measured[name] = ctypes.sizeof(record)
    measured[f"{name}.alignment"] = ctypes.alignment(record)
    for field_name, ctype in fields:
      key = f"{name}.{field_name}"
      measured[key] = getattr(record, field_name).offset
      measured[key + ".size"] = ctypes.sizeof(ctype)
      measured[key + ".alignment"] = ctypes.alignment(ctype)
      if issubclass(ctype, ctypes.Array):
        measured[key + ".extent"] = ctype._length_
        measured[key + ".stride"] = ctypes.sizeof(ctype._type_)
  return structs, measured


def compare_layouts(
  c_structs: Dict[str, Fields],
  py_structs: Dict[str, Fields],
  ground_truth: Dict[str, int],
  *,
  measured: Dict[str, int],
) -> List[str]:
  """Reconcile record/field sets, order, exact types and independently measured storage."""
  failures = []
  for name in sorted(c_structs.keys() - py_structs.keys()):
    failures.append(f"{name}: no ctypes mirror in common.py")
  for name in sorted(py_structs.keys() - c_structs.keys()):
    failures.append(f"{name}: ctypes mirror without a struct in celestial.h")
  for name in sorted(c_structs.keys() & py_structs.keys()):
    if c_structs[name] != py_structs[name]:
      failures.append(f"{name}: field schema/order {c_structs[name]} (C) != {py_structs[name]} (ctypes)")

  for key in sorted(ground_truth.keys() & measured.keys()):
    if ground_truth[key] != measured[key]:
      failures.append(f"{key}: {ground_truth[key]} (C) != {measured[key]} (ctypes)")
  return failures


def check_runtime_layout(module: object) -> int:
  """Compare loaded ctypes records with compiled C."""
  header = paths.proj_root() / "src/shared_lib/celestial.h"
  try:
    c_structs = parse_c_structs(header)
    runtime_structs, measured = runtime_layout(module)
  except (RuntimeError, AttributeError, TypeError, ValueError) as error:
    red_print(f"Loaded ctypes mirror differs from C: {error}")
    return 1

  with tempfile.TemporaryDirectory(prefix="ctypes_runtime_") as directory:
    ground_truth = c_layout(c_structs, header, Path(directory))
  if ground_truth is None:
    return 1

  failures = compare_layouts(c_structs, runtime_structs, ground_truth, measured=measured)
  if failures:
    for failure in failures:
      red_print(failure)
    return 1

  green_print(f"Loaded ctypes records agree with compiled C ({len(c_structs)} structs)")
  return 0


def check_abi_layout() -> int:
  """Hold the `statistics/common.py` ctypes mirror to the real layout in `celestial.h`.

  Every struct in `celestial.h` has a ctypes mirror in `common.py`, and a struct read back
  through a drifted mirror is garbage no test prints (#85). Three layers, because
  each sees what the others cannot: set equality catches a missing or extra mirror;
  size/alignment/offset/extent/stride catch layout drift; exact field types catch what
  raw layout cannot -- signedness and same-width swaps (c_int32 vs c_uint32, c_bool vs c_uint8).
  """
  print("#" * 60)
  yellow_print("Checking that the ctypes mirror matches the real ABI layout...")

  header = paths.proj_root() / "src" / "shared_lib" / "celestial.h"
  mirror = paths.proj_root() / "statistics" / "common.py"

  try:
    c_structs = parse_c_structs(header)
    py_structs = parse_py_structs(mirror)
  except (RuntimeError, SyntaxError) as e:
    red_print(f"Parse failed: {e}")
    return 1

  # Layer 2: compile and run the ground-truth probe once for every struct.
  with tempfile.TemporaryDirectory(prefix="abi_layout_") as tmp:
    ground_truth = c_layout(c_structs, header, Path(tmp))
  if ground_truth is None:
    return 1

  failures = compare_layouts(c_structs, py_structs, ground_truth, measured=py_layout(py_structs))

  print("#" * 60)
  if failures:
    red_print(f"ABI layout gate failed ({len(failures)} finding(s)):")
    for f in failures:
      red_print(f"  - {f}")
    return 1

  green_print(f"ctypes mirror matches the ABI layout ({len(c_structs)} structs)")
  return 0
