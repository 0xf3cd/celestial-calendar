# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""The supported declaration subset of celestial.h; unconsumed text is an error."""

import re
from dataclasses import dataclass
from pathlib import Path
from typing import Final


C_TO_CTYPES: Final[dict[str, str]] = {
  "bool": "c_bool",
  "uint8_t": "c_uint8",
  "uint16_t": "c_uint16",
  "uint32_t": "c_uint32",
  "int32_t": "c_int32",
  "double": "c_double",
}

Fields = list[tuple[str, str]]

STRUCT_RE = re.compile(r"typedef\s+struct\s+([A-Za-z_]\w*)\s*\{([^{}]*)\}\s*\1\s*;", re.DOTALL)
FIELD_RE = re.compile(r"([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*(?:\[\s*([1-9][0-9]*)\s*\])?")


@dataclass(frozen=True)
class CType:
  base: str
  const: bool = False
  pointer: bool = False
  pointer_const: bool = False

  def spelling(self) -> str:
    return (
      ("const " if self.const else "")
      + self.base
      + (" *" + (" const" if self.pointer_const else "") if self.pointer else "")
    )

  def declaration(self, name: str) -> str:
    return self.spelling() + ("" if self.pointer and not self.pointer_const else " ") + name


@dataclass(frozen=True)
class Parameter:
  name: str
  type: CType


@dataclass(frozen=True)
class Function:
  name: str
  result: CType
  parameters: tuple[Parameter, ...]
  exported: bool

  def signature(self) -> str:
    arguments = ", ".join(p.type.declaration(p.name) for p in self.parameters) or "void"
    return f"{self.result.declaration(self.name)}({arguments})"


@dataclass(frozen=True)
class Header:
  structs: dict[str, Fields]
  functions: dict[str, Function]
  constants: dict[str, int]
  aliases: dict[str, str]


def strip_comments(text: str) -> str:
  """Preserve literals so comment-looking text inside a string cannot hide declarations."""
  tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*'
  return re.sub(tokens, lambda m: " " if m[0].startswith(("/*", "//")) else m[0], text, flags=re.S)


def parse_type(text: str, records: dict[str, Fields]) -> CType:
  match = re.fullmatch(r"\s*(const\s+)?([A-Za-z_]\w*)\s*(?:(\*)\s*(const)?)?\s*", text)
  if match is None:
    raise RuntimeError(f"Unsupported C type: {text!r}")
  const, base, pointer, pointer_const = match.groups()
  if base not in C_TO_CTYPES and base not in records and base not in {"char", "void"}:
    raise RuntimeError(f"Unknown C type: {base}")
  if base == "char" and not pointer or const and not pointer or base == "void" and pointer:
    raise RuntimeError(f"Unsupported C type: {text!r}")
  return CType(base, bool(const), bool(pointer), bool(pointer_const))


def _without_scaffold(text: str) -> str:
  """Accept the header's fixed portability scaffolding, not arbitrary conditional declarations."""
  text = strip_comments(text).strip()
  if text.startswith("#ifndef CELESTIAL_CALENDAR_H"):
    opening = r"#ifndef CELESTIAL_CALENDAR_H\s+#define CELESTIAL_CALENDAR_H\s+"
    text, count = re.subn(opening, "", text, count=1)
    if count != 1 or not text.endswith("#endif"):
      raise RuntimeError("Unsupported C header guard")
    text = text[: -len("#endif")].strip()
  scaffold = """#ifdef _WIN32
  #if defined(CELESTIAL_BUILDING_DLL)
    #define CELESTIAL_API __declspec(dllexport)
  #else
    #define CELESTIAL_API __declspec(dllimport)
  #endif
#elif defined(__GNUC__) || defined(__clang__)
  #define CELESTIAL_API __attribute__((visibility("default")))
#else
  #define CELESTIAL_API
#endif"""
  text = re.sub(r"\A(?:#include <(?:stdbool|stdint)\.h>\s*)+", "", text).strip()
  pattern = r"\s+".join(re.escape(word) for word in scaffold.split())
  text = re.sub(r"\A" + pattern, "", text).strip()
  opening = r'#ifdef __cplusplus\s+extern "C" \{\s+#endif'
  closing = r"#ifdef __cplusplus\s+\}\s+#endif\s*\Z"
  text, opened = re.subn(r"\A" + opening, "", text)
  if opened:
    text, closed = re.subn(closing, "", text)
    if not closed:
      raise RuntimeError("Missing C++ linkage closing block")
  if not text.isascii() or "\\" in text:
    raise RuntimeError("Unsupported non-ASCII or escaped C declaration")
  return text


def parse_function(declaration: str, structs: dict[str, Fields], *, exported: bool = True) -> Function:
  """Parse one prototype's spelling using the same type rules as the complete header."""
  function = re.fullmatch(r"(.+?)([A-Za-z_]\w*)\s*\(([^()]*)\)\s*", declaration, flags=re.S)
  if function is None:
    raise RuntimeError(f"Unsupported function declaration: {declaration!r}")
  result, name, arguments = function.groups()
  result = parse_type(result, structs)
  parameters = []
  for argument in [] if arguments.strip() == "void" else arguments.split(","):
    parameter = re.fullmatch(r"(.+?)([A-Za-z_]\w*)\s*", argument, flags=re.S)
    if parameter is None:
      raise RuntimeError(f"Unsupported parameter in {name}: {argument!r}")
    kind, parameter_name = parameter.groups()
    kind = parse_type(kind, structs)
    if kind.base == "void" or parameter_name in {p.name for p in parameters}:
      raise RuntimeError(f"Invalid parameter in {name}: {argument!r}")
    parameters.append(Parameter(parameter_name, kind))
  return Function(name, result, tuple(parameters), exported)


def split_field_type(value: str) -> tuple[str, int | None]:
  """Split a canonical scalar/record type or one positive decimal-literal array."""
  match = re.fullmatch(r"([A-Za-z_]\w*)(?:\[([1-9][0-9]*)\])?", value)
  if match is None:
    raise RuntimeError(f"Unsupported ABI field type: {value!r}")
  base, extent = match.groups()
  return base, None if extent is None else int(extent)


def parse_header_text(header: str) -> Header:
  """Consume named records, full prototypes and uint32 numeric/alias macros."""
  header = _without_scaffold(header)
  structs: dict[str, Fields] = {}
  functions: dict[str, Function] = {}
  constants: dict[str, int] = {}
  aliases: dict[str, str] = {}
  while header.strip():
    header = header.lstrip()
    if header.startswith("#define "):
      line, _, header = header.partition("\n")
      macro = re.fullmatch(
        r"#define ([A-Z][A-Z0-9_]*)\s+(?:UINT32_C\((0|[1-9][0-9]*|0x[0-9A-Fa-f]+)\)|([A-Z][A-Z0-9_]*))",
        line.rstrip(),
      )
      if macro is None:
        raise RuntimeError(f"Unsupported public macro: {line!r}")
      name, value, alias = macro.groups()
      if name in constants or name in structs or name in functions:
        raise RuntimeError(f"Duplicate public macro: {name}")
      if alias is not None:
        if alias not in constants:
          raise RuntimeError(f"Unknown or forward public macro alias: {alias}")
        aliases[name] = alias
      constants[name] = constants[alias] if alias is not None else int(value, 16 if value.startswith("0x") else 10)
      if constants[name] > 0xFFFFFFFF:
        raise RuntimeError(f"Public macro exceeds uint32_t: {name}")
      continue
    if not header.startswith("typedef"):
      prototype = re.match(r"(CELESTIAL_API\s+)?([^;{}]+);", header)
      if prototype is None:
        raise RuntimeError(f"Unsupported C declaration near {header[:80]!r}")
      exported, declaration = prototype.groups()
      function = parse_function(declaration, structs, exported=bool(exported))
      name = function.name
      if name in functions or name in structs or name in constants:
        raise RuntimeError(f"Duplicate C declaration: {name}")
      functions[name] = function
      header = header[prototype.end() :]
      continue
    match = STRUCT_RE.match(header)
    if match is None:
      raise RuntimeError(f"Unsupported typedef struct near {header[:80]!r}")
    name, body = match.groups()
    if name in structs or name in C_TO_CTYPES or name in functions or name in constants:
      raise RuntimeError(f"Duplicate or reserved struct name: {name}")
    declarations = body.strip().split(";")
    if declarations[-1].strip():
      raise RuntimeError(f"Unterminated field declaration in {name}")

    fields: Fields = []
    seen: set[str] = set()
    for declaration in declarations[:-1]:
      field = FIELD_RE.fullmatch(declaration.strip())
      if field is None:
        raise RuntimeError(f"Unsupported field declaration in {name}: {declaration.strip()!r}")
      base, field_name, extent = field.groups()
      if base not in C_TO_CTYPES and base not in structs:
        raise RuntimeError(f"Unknown or not previously declared field type in {name}.{field_name}: {base}")
      if field_name in seen:
        raise RuntimeError(f"Duplicate field in {name}: {field_name}")
      seen.add(field_name)
      fields.append((field_name, base if extent is None else f"{base}[{extent}]"))
    if not fields:
      raise RuntimeError(f"No fields parsed from struct {name}")
    structs[name] = fields
    header = header[match.end() :]

  return Header(structs, functions, constants, aliases)


def parse_c_structs_text(header: str) -> dict[str, Fields]:
  structs = parse_header_text(header).structs
  if not structs:
    raise RuntimeError("No typedef struct found")
  return structs


def parse_header(header: Path) -> Header:
  return parse_header_text(header.read_text(encoding="utf-8"))


def parse_c_structs(header: Path) -> dict[str, Fields]:
  """Parse the published header without compiling or loading a native library."""
  return parse_c_structs_text(header.read_text(encoding="utf-8"))


def record_layouts(structs: dict[str, Fields], view: str) -> dict:
  """Natural native64/wasm32 layout rules; compiler probes validate both views."""
  if view not in {"python", "javascript"}:
    raise ValueError(f"Unknown ABI view: {view}")
  primitives = {
    "bool": (1, 1),
    "uint8_t": (1, 1),
    "uint16_t": (2, 2),
    "int32_t": (4, 4),
    "uint32_t": (4, 4),
    "double": (8, 8),
  }
  layouts = {}
  for name, schema in structs.items():
    offset, alignment = 0, 1
    fields = []
    for field_name, field_type in schema:
      base, extent = split_field_type(field_type)
      size, align = primitives[base] if base in primitives else (layouts[base]["size"], layouts[base]["alignment"])
      offset = (offset + align - 1) // align * align
      field = {"name": field_name, "type": field_type, "offset": offset}
      if extent is not None and view == "python":
        field.update(extent=extent, stride=size)
      fields.append(field)
      offset += size * (extent or 1)
      alignment = max(alignment, align)
    layouts[name] = {
      "size": (offset + alignment - 1) // alignment * alignment,
      "alignment": alignment,
      "fields": fields,
    }
  return layouts
