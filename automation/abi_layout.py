# CelestialCalendar Automation:
#   Python automation scripts for building and testing the CelestialCalendar C++ project.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import ctypes
import importlib.util
import os
import shutil
import tempfile

from pathlib import Path
from types import ModuleType
from typing import Dict, List, Optional, Tuple

from . import paths
from .c_abi_schema import C_TO_CTYPES, Fields, parse_c_structs, split_field_type
from .utils import run_cmd, green_print, red_print, yellow_print


def load_abi() -> ModuleType:
  """Load the real declarations without importing the package or its native loader."""
  source = paths.proj_root() / "bindings/python/src/celestial_calendar/_abi.py"
  spec = importlib.util.spec_from_file_location("_celestial_abi_declarations", source)
  if spec is None or spec.loader is None:
    raise RuntimeError(f"Cannot load ABI declarations from {source}")
  module = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(module)
  return module


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


def runtime_layout(module: object, *, library: Optional[object] = None) -> Tuple[Dict[str, Fields], Dict[str, int]]:
  """Measure actual records and check declared and loaded return-record identity."""
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
  registry = getattr(module, "STRUCT_TYPES", None)
  if registry is not None and registry != records:
    raise RuntimeError("Runtime record registry differs from its bound classes")
  primitives = {getattr(ctypes, python): c for c, python in C_TO_CTYPES.items()}

  for name, (argtypes, restype) in getattr(module, "BINDING_SPECS", {}).items():
    if isinstance(restype, type) and issubclass(restype, ctypes.Structure) and restype not in names:
      raise RuntimeError(f"Unmeasured declared return record: {name}")
    if library is not None:
      function = getattr(library, name)
      if function.restype is not restype or tuple(function.argtypes) != tuple(argtypes):
        raise RuntimeError(f"Loaded signature differs from its declaration: {name}")
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
    failures.append(f"{name}: no ctypes declaration")
  for name in sorted(py_structs.keys() - c_structs.keys()):
    failures.append(f"{name}: ctypes mirror without a struct in celestial.h")
  for name in sorted(c_structs.keys() & py_structs.keys()):
    if c_structs[name] != py_structs[name]:
      failures.append(f"{name}: field schema/order {c_structs[name]} (C) != {py_structs[name]} (ctypes)")

  for key in sorted(ground_truth.keys() & measured.keys()):
    if ground_truth[key] != measured[key]:
      failures.append(f"{key}: {ground_truth[key]} (C) != {measured[key]} (ctypes)")
  return failures


def check_runtime_layout(module: object, *, library: Optional[object] = None) -> int:
  """Compare loaded ctypes records with compiled C."""
  header = paths.proj_root() / "src/shared_lib/celestial.h"
  try:
    c_structs = parse_c_structs(header)
    runtime_structs, measured = runtime_layout(module, library=library)
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
  """Compare the package's real declarations with compiled C, without loading a library."""
  print("#" * 60)
  yellow_print("Checking that the ctypes mirror matches the real ABI layout...")

  header = paths.proj_root() / "src" / "shared_lib" / "celestial.h"
  try:
    c_structs = parse_c_structs(header)
    py_structs, measured = runtime_layout(load_abi())
  except (RuntimeError, SyntaxError, AttributeError, TypeError, ValueError) as e:
    red_print(f"Parse failed: {e}")
    return 1

  # Compile and run the ground-truth probe once for every struct.
  with tempfile.TemporaryDirectory(prefix="abi_layout_") as tmp:
    ground_truth = c_layout(c_structs, header, Path(tmp))
  if ground_truth is None:
    return 1

  failures = compare_layouts(c_structs, py_structs, ground_truth, measured=measured)

  print("#" * 60)
  if failures:
    red_print(f"ABI layout gate failed ({len(failures)} finding(s)):")
    for f in failures:
      red_print(f"  - {f}")
    return 1

  green_print(f"ctypes mirror matches the ABI layout ({len(c_structs)} structs)")
  return 0
