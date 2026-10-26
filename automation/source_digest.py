#!/usr/bin/env python3
#
# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

import ast
import hashlib
import json
import re
import unicodedata

from pathlib import Path


RETAINED_REGISTRY = Path("src/test/provenance/batch-a-closeout/retained_host_blocks.json")
RETAINED_DATA_SHA256 = "67e0cb02a607a143ff75696348455bc431382a1d54e39a6e5813e9739c4f6ec3"
# Add new value-bearing type/member spellings here; implementation names and test macros stay free.
REFERENCE_VALUE_PATTERN = re.compile(
  r'"(?:\\.|[^"\\])*"|(?<![\w.])(?:0x[0-9a-fA-F]+|\d+(?:\.\d*)?(?:[eE][+-]?\d+)?|\.\d+)'
  r"(?:[a-zA-Z_]\w*)?|\b(?:true|false)\b|[+*/-]|\b[A-Za-z_]\w*(?:::\w+)+"
  r"|\b(?:Angle|AngleDeg|AngleRad|DistanceKm|DistanceAu|GeoLocation|Datetime|Params|TraditionalMonth|RAD|DEG|AU|KM)\b"
  r"|\.(?:latitude|longitude|lat_deg|lon_deg|λ|β|α|δ|r|model|deg|Mp|A1|leap_month|angle_deg|"
  r"jd_ut1|jde_tt|ymd|fraction)\b"
)


def canonical_cpp(text: str) -> str:
  without_comments = re.sub(r"//[^\n]*|/\*.*?\*/", " ", text, flags=re.DOTALL)
  return " ".join(without_comments.split())


CPP_TOKEN_ALGORITHM = "cpp-preprocessing-tokens"
CPP_TOKEN_VERSION = 1

_CPP_SPLICE = re.compile(r"\\[ \t\v\f]*\n")
_CPP_PUNCTUATORS = tuple(
  ">>= <<= <=> ->* ... ## :: .* -> ++ -- << >> <= >= == != && || += -= *= /= %= ^= &= |=".split()
) + tuple("{}[]();:?.+-*/%^&|~!=<>,#")
_CPP_DIGRAPHS = ("<:", ":>", "<%", "%>", "%:")
_CPP_DIRECTIVES = frozenset(
  {
    "define",
    "elif",
    "elifdef",
    "elifndef",
    "else",
    "endif",
    "error",
    "if",
    "ifdef",
    "ifndef",
    "include",
    "line",
    "pragma",
    "undef",
    "warning",
  }
)
_CPP_LITERAL_PREFIXES = (
  'u8R"',
  'uR"',
  'UR"',
  'LR"',
  'R"',
  'u8"',
  'u"',
  'U"',
  'L"',
  '"',
  "u8'",
  "u'",
  "U'",
  "L'",
  "'",
)
_CPP_LOCATION_IDENTIFIERS = frozenset(
  {
    "__LINE__",
    "__FILE__",
    "__BASE_FILE__",
    "__FILE_NAME__",
    "__INCLUDE_LEVEL__",
    "__COUNTER__",
    "__DATE__",
    "__TIME__",
    "__TIMESTAMP__",
    "__builtin_LINE",
    "__builtin_FILE",
    "__builtin_COLUMN",
    "__builtin_source_location",
    "source_location",
  }
)


def _cpp_identifier_start(char: str) -> bool:
  return bool(char) and (char == "_" or char.isalpha())


def _cpp_identifier_continue(char: str) -> bool:
  return bool(char) and (_cpp_identifier_start(char) or char.isdigit() or unicodedata.category(char) in {"Mn", "Mc"})


class _CppTokenReader:
  """Read logical characters outside raw literals; keep their physical bodies intact."""

  def __init__(self, text: str) -> None:
    self.text = text.replace("\r\n", "\n").replace("\r", "\n")
    self.position = 0

  def step(self, position: int) -> tuple[str, int]:
    while (
      position < len(self.text) and self.text[position] == "\\" and (splice := _CPP_SPLICE.match(self.text, position))
    ):
      position = splice.end()
    if position == len(self.text):
      return "", position
    return self.text[position], position + 1

  def peek(self, offset: int = 0) -> str:
    position = self.position
    char = ""
    for _ in range(offset + 1):
      char, position = self.step(position)
    return char

  def take(self) -> str:
    char, self.position = self.step(self.position)
    return char

  def matches(self, spelling: str) -> bool:
    position = self.position
    for expected in spelling:
      char, position = self.step(position)
      if char != expected:
        return False
    return True

  def identifier(self) -> str:
    result = self.take()
    while _cpp_identifier_continue(self.peek()):
      result += self.take()
    return result

  def number(self) -> str:
    result = self.take()
    while char := self.peek():
      if _cpp_identifier_continue(char) or char == ".":
        result += self.take()
      elif char == "'" and _cpp_identifier_continue(self.peek(1)):
        result += self.take() + self.take()
      elif char in "+-" and result[-1] in "eEpP":
        result += self.take()
      else:
        break
    return result

  def literal(self, prefix: str) -> str:
    result = "".join(self.take() for _ in prefix)
    if prefix.endswith('R"'):
      begin = self.position
      opening = self.text.find("(", begin)
      if opening < 0 or opening - begin > 16:
        raise ValueError("Invalid C++ raw-string delimiter")
      delimiter = self.text[begin:opening]
      if any(char.isspace() or char in "()\\" or ord(char) < 32 for char in delimiter):
        raise ValueError("Invalid C++ raw-string delimiter")
      closing = self.text.find(")" + delimiter + '"', opening + 1)
      if closing < 0:
        raise ValueError("Unterminated C++ raw string")
      self.position = closing + len(delimiter) + 2
      result += self.text[begin : self.position]
    else:
      quote = prefix[-1]
      while True:
        char = self.take()
        if not char or char == "\n":
          raise ValueError("Unterminated C++ quoted literal")
        result += char
        if char == quote:
          break
        if char == "\\":
          escaped = self.take()
          if not escaped or escaped == "\n":
            raise ValueError("Unterminated C++ literal escape")
          result += escaped
    if _cpp_identifier_start(self.peek()):
      result += self.identifier()
    return result

  def header_name(self) -> str:
    result = self.take()
    closing = ">" if result == "<" else '"'
    while True:
      char = self.take()
      if not char or char == "\n":
        raise ValueError("Unterminated C++ header name")
      result += char
      if char == closing:
        return result


def _cpp_tokens(text: str) -> list[tuple[str, str]]:
  reader = _CppTokenReader(text)
  tokens = []
  directive: list[str] | None = None
  line_start = True
  gap = False

  while char := reader.peek():
    if char in " \t\v\f\n":
      reader.take()
      gap = True
      if char == "\n":
        if directive is not None:
          tokens.append(("directive-end", ""))
          directive = None
        line_start = True
      continue
    if reader.matches("//"):
      reader.take()
      reader.take()
      while reader.peek() not in {"", "\n"}:
        reader.take()
      gap = True
      continue
    if reader.matches("/*"):
      reader.take()
      reader.take()
      while not reader.matches("*/"):
        if not reader.take():
          raise ValueError("Unterminated C++ block comment")
      reader.take()
      reader.take()
      gap = True
      continue

    prefix = (
      next((prefix for prefix in _CPP_LITERAL_PREFIXES if reader.matches(prefix)), None) if char in "\"'uULR" else None
    )
    header = directive is not None and (directive == ["#", "include"] or directive[-2:] == ["__has_include", "("])
    if header and char not in '<"':
      raise ValueError("Unsupported C++ computed header operand")
    if header and char in '<"':
      value = reader.header_name()
    elif prefix is not None:
      value = reader.literal(prefix)
    elif _cpp_identifier_start(char):
      value = reader.identifier()
      if value in _CPP_LOCATION_IDENTIFIERS:
        raise ValueError(f"Unsupported location-sensitive C++ identifier: {value}")
      if value in {"__has_include_next", "__has_embed", "import"}:
        raise ValueError(f"Unsupported C++ header import or extension: {value}")
    elif char.isdigit() or char == "." and reader.peek(1).isdigit():
      value = reader.number()
    else:
      # [lex.pptoken]: <:: starts < then :: unless its next character is : or >.
      leading_global_argument = reader.matches("<::") and reader.peek(3) not in {":", ">"}
      if not leading_global_argument and any(reader.matches(token) for token in _CPP_DIGRAPHS):
        raise ValueError("Unsupported C++ alternative preprocessing token")
      spelling = (
        "<"
        if leading_global_argument
        else next((token for token in _CPP_PUNCTUATORS if token[0] == char and reader.matches(token)), None)
      )
      if spelling is None:
        raise ValueError(f"Unsupported C++ source character: {char!r}")
      value = "".join(reader.take() for _ in spelling)

    if directive == ["#"] and value not in _CPP_DIRECTIVES:
      raise ValueError(f"Unsupported C++ preprocessing directive: {value}")
    if value == "#" and line_start:
      directive = ["#"]
      tokens.append(("directive", "#"))
    else:
      if directive is not None and value in {"#", "##"}:
        raise ValueError("Unsupported C++ stringification or token-pasting directive")
      macro_open = directive is not None and len(directive) == 3 and directive[1] == "define" and value == "("
      tokens.append(("function-macro-open" if macro_open and not gap else "token", value))
      if directive is not None:
        directive.append(value)
    line_start = False
    gap = False

  if directive is not None:
    tokens.append(("directive-end", ""))
  return tokens


def validate_cpp_token_algorithm(algorithm: str, version: int) -> None:
  if algorithm != CPP_TOKEN_ALGORITHM or type(version) is not int or version != CPP_TOKEN_VERSION:
    raise ValueError(f"Unsupported C++ token digest: {algorithm!r}, version {version!r}")


def cpp_token_digest(text: str, algorithm: str = CPP_TOKEN_ALGORITHM, version: int = CPP_TOKEN_VERSION) -> str:
  """Hash C++ token spellings, directive boundaries and macro-name/parenthesis adjacency.

  LF and CRLF are equivalent. Literal contents and token boundaries are retained;
  comments and other whitespace are ignored. Adjacency distinguishes function-like #define
  directives. This is not macro expansion: digraphs, stringification, token pasting,
  Unicode-escape and location-sensitive identifiers are rejected. Header operands must be
  literal names; directive names are limited to standard C++23 forms, with no header-unit imports.
  """
  validate_cpp_token_algorithm(algorithm, version)
  return hashlib.sha256(json.dumps(_cpp_tokens(text), ensure_ascii=False, separators=(",", ":")).encode()).hexdigest()


def retained_slice(text: str, start: str, end: str) -> str:
  """Retained data markers bind whole blocks; reference inputs/values markers bind value atoms."""
  if text.count(start) != 1:
    raise RuntimeError(f"retained data start is missing or ambiguous: {start}")
  begin = text.index(start)
  finish = text.find(end, begin + len(start))
  if finish < 0:
    raise RuntimeError(f"retained data end is missing: {end}")
  return text[begin:finish]


def retained_digest(path: Path, method: str, start: str = "", end: str = "", input_facts: tuple[str, ...] = ()) -> str:
  if method == "raw_sha256":
    data = path.read_bytes()
  elif method == "notebook_values":
    notebook = json.loads(path.read_text(encoding="utf-8"))
    # Execution counters and presentation metadata do not identify computed values.
    values = [
      {key: value for key, value in output.items() if key not in {"metadata", "execution_count"}}
      for cell in notebook["cells"]
      for output in cell.get("outputs", [])
    ]
    inputs = []
    for cell in notebook["cells"]:
      if cell["cell_type"] != "code":
        continue
      source = "".join(cell["source"])
      try:
        tree = ast.parse(source)
      except SyntaxError as error:
        if re.search(r"\bJieqi\s*\.", source):
          raise RuntimeError("retained notebook input-bearing cell cannot be parsed") from error
        continue
      for node in ast.walk(tree):
        if not isinstance(node, ast.List) or not node.elts:
          continue
        pairs = [
          item
          for item in node.elts
          if isinstance(item, ast.Call)
          and len(item.args) == 2
          and isinstance(item.args[0], ast.Constant)
          and type(item.args[0].value) is int
          and isinstance(item.args[1], ast.Attribute)
          and isinstance(item.args[1].value, ast.Name)
          and item.args[1].value.id == "Jieqi"
        ]
        if len(pairs) == len(node.elts):
          require_facts(source, input_facts, "notebook retained input source")
          inputs.append([(item.args[0].value, item.args[1].attr) for item in pairs])
    data = json.dumps(
      {"outputs": values, "directed_jieqi_inputs": inputs}, sort_keys=True, separators=(",", ":")
    ).encode()
  elif method in {"canonical_cpp", "cpp_reference_values"}:
    text = path.read_text(encoding="utf-8")
    retained = canonical_cpp(retained_slice(text, start, end))
    if method == "cpp_reference_values":
      # Printed examples carry values in calls; units and coordinate labels identify their inputs too.
      retained = " ".join(REFERENCE_VALUE_PATTERN.findall(retained))
    data = retained.encode()
  else:
    raise RuntimeError(f"unknown retained-digest method: {method}")
  return hashlib.sha256(data).hexdigest()


def json_digest(value: object) -> str:
  return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def verify_retained_data(repo_root: Path, ids: tuple[str, ...] | None = None) -> int:
  blocks = json.loads((repo_root / RETAINED_REGISTRY).read_text(encoding="utf-8"))["data_blocks"]
  if json_digest(blocks) != RETAINED_DATA_SHA256:
    raise RuntimeError("retained data identity record differs")
  selected = tuple(blocks) if ids is None else ids
  for block_id in selected:
    block = blocks[block_id]
    path = Path(block["path"])
    if path.is_absolute() or ".." in path.parts:
      raise RuntimeError(f"{block_id} retained path must be repository-relative")
    digest = retained_digest(
      repo_root / path,
      block["method"],
      block.get("start", ""),
      block.get("end", ""),
      tuple(block.get("input_facts", ())),
    )
    if digest != block["sha256"]:
      raise RuntimeError(f"{block_id} retained data hash differs: {digest}")
  return len(selected)


def require_facts(text: str, facts: tuple[str, ...] | list[str], label: str) -> None:
  normalized = " ".join(re.sub(r"(?m)^\s*(?://|#|\*)\s?", "", text).split())
  for fact in facts:
    if fact not in normalized:
      raise RuntimeError(f"{label} required fact is missing: {fact}")


def attribution_digest(text: str) -> str:
  """Identify cited URLs, literal identities and quantities, independently of project prose."""
  atoms = re.findall(
    r"https?://[^\s`)>]+|`[^`]+`|(?<![\w])(?:[0-9a-f]{40,64}|\d+(?:[.,]\d+)*(?:[eE][+-]?\d+)?)(?![\w])",
    text,
  )
  return json_digest(sorted(atoms))
