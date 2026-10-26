# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import pytest

from automation.source_digest import CPP_TOKEN_ALGORITHM, CPP_TOKEN_VERSION, cpp_token_digest


def test_cpp_token_digest_version_one_serialization():
  assert CPP_TOKEN_ALGORITHM == "cpp-preprocessing-tokens"
  assert CPP_TOKEN_VERSION == 1
  assert cpp_token_digest("int x;") == "8434098a6de753ed9d4a51f8032689c319437d9b23c11a6eef8c45c0ab170083"


@pytest.mark.parametrize(
  ("before", "after"),
  [
    ("const double x = 1;", " /* note */ const\n double\tx = 1; // end\n"),
    ("int x;", "int/**/x;"),
    ("int x;", "int/*\nseveral\nlines*/x;"),
    ("int x;", "int x;\\\n"),
    ("std::vector<::std::size_t> v;", "std::vector< ::std::size_t> v;"),
    ("T<::X>", "T<:\\\n:X>"),
    ("int x;", "int x;\\ \t\n"),
    ("int value;", "int va\\\nlue;"),
    ("a += b;", "a +\\\n= b;"),
    ("int a; // hidden\nint b;", "int a; // hidden\\\n still hidden\nint b;"),
    ("int a; /* hidden */ int b;", "int a; /\\\n* hidden *\\\n/ int b;"),
    ('auto s = "ab";', 'auto s = "a\\\nb";'),
    ('auto s = R"(ab)";', 'auto s = R\\\n"(ab)";'),
    ('auto s = R"(ab)";', 'auto s = R"(ab)"\\\n;'),
    ("#define F(x) x\nF(1)", " # define F(x) /* note */ x\n\nF ( 1 )\n"),
    ("#define F (x) x\n", "#define F/**/(x) x\n"),
    ("#define F (x) x\n", "#define F/*\n*/(x) x\n"),
    ("#define X 1 + 2\n", "#define X 1 +\\\n 2\n"),
    ("#define X 1", "#define X 1\n"),
    ("#define X 1\n", "#define X 1\\\n"),
    ("#define X 1\n", "#define X 1 /*\n*/\n"),
    ("#include <array>\n", " # include /* comment */ <array>\n"),
    ("#include <array>\n", "#include <ar\\\nray>\n"),
    ('#include "a.hpp"\n', '#include "a.hpp" // note\n'),
    ("#if __has_include(<array>)\n#endif\n", "#if __has_include ( <array> )\n#endif\n"),
    ('auto λ = "https://a/*x*/";\n', 'auto λ = "https://a/*x*/";\r\n'),
    ('auto s = R"(a\nb)";\n', 'auto s = R"(a\r\nb)";\r\n'),
    ('auto s = "x"_tag;', 'auto s = "x"\\\n_tag;'),
    ("auto x = 0x1.2p-3;", "auto x = 0x1.2p-\\\n3;"),
  ],
)
def test_cpp_token_digest_ignores_only_nonsemantic_layout(before, after):
  assert cpp_token_digest(before) == cpp_token_digest(after)


@pytest.mark.parametrize(
  ("before", "after"),
  [
    ('auto url = "https://a"; int x = 1;', 'auto url = "https://a"; int x = 2;'),
    ('auto s = "/* a */";', 'auto s = "/* b */";'),
    ('auto s = "a  b";', 'auto s = "a b";'),
    ('auto s = "a\\"b";', 'auto s = "a\\"c";'),
    ('auto s = R"tag(a // b /* c */)tag";', 'auto s = R"tag(a // b /* d */)tag";'),
    ('auto s = R"(a  b)";', 'auto s = R"(a b)";'),
    ('auto s = R"(a\\\nb)";', 'auto s = R"(ab)";'),
    ('auto s = R"(a\nb)";', 'auto s = R"(ab)";'),
    ('auto s = R"(a\\\nb)";', 'auto s = R"(a\nb)";'),
    ("auto c = '\\'';", "auto c = '\"';"),
    ("auto c = ' ';", "auto c = '\\t';"),
    ("auto c = u'a';", "auto c = U'a';"),
    ('auto s = u8"a";', 'auto s = "a";'),
    ('auto s = "x"_tag;', 'auto s = "x" _tag;'),
    ('auto s = "x"_tag;', 'auto s = "x"/**/_tag;'),
    ('auto s = "a" "b";', 'auto s = "ab";'),
    ("a + +b;", "a ++b;"),
    ("a < < b;", "a << b;"),
    ("a - > b;", "a -> b;"),
    ("a / * b;", "a /* b */;"),
    ("float x;", "floatx;"),
    ("auto x = 1e+2;", "auto x = 1e + 2;"),
    ("auto x = 1'000;", "auto x = 1000;"),
    ("auto x = 1.0;", "auto x = 1.1;"),
    ("a.b;", "a .1;"),
    ("auto λ = 1;", "auto β = 1;"),
    ("#define F(x) x\n", "#define F (x) x\n"),
    ("#define F(x) x\n", "#define F/**/(x) x\n"),
    ("#define F(x) x\n", "#define F/*\n*/(x) x\n"),
    ("#define X 1\n+2\n", "#define X 1 +2\n"),
    ("int a;\n#define X 1\n", "int a; #define X 1\n"),
    ("#define X 1\nint b;", "#define X 1\\\nint b;"),
    ("#include <a/b>\n", "#include <a / b>\n"),
    ('#include "a  b"\n', '#include "a b"\n'),
    ("#if __has_include(<a/b>)\n", "#if __has_include(<a / b>)\n"),
    ("#include <array>\n", "#include <vector>\n"),
    ("int a; // comment\nint b;", "int a; // comment\\\nint b;"),
  ],
)
def test_cpp_token_digest_preserves_tokens_and_directive_structure(before, after):
  assert cpp_token_digest(before) != cpp_token_digest(after)


@pytest.mark.parametrize("prefix", ["", "u8", "u", "U", "L"])
def test_cpp_token_digest_preserves_prefixed_literal_bodies(prefix):
  for quote in ['"', "'"]:
    assert cpp_token_digest(f"{prefix}{quote} {quote}") != cpp_token_digest(f"{prefix}{quote}x{quote}")
  assert cpp_token_digest(f'{prefix}R"(a\\\nb)"') != cpp_token_digest(f'{prefix}R"(ab)"')


@pytest.mark.parametrize(
  "source",
  [
    "/* open",
    '"open',
    '"escape\\',
    "'open",
    'R"(open',
    'R"bad delimiter(x)bad delimiter"',
    'R"12345678901234567(x)12345678901234567"',
    "#include <open",
    '#include "open',
    "int a;\\",
  ],
)
def test_cpp_token_digest_rejects_unterminated_or_unsupported_source(source):
  with pytest.raises(ValueError):
    cpp_token_digest(source)


@pytest.mark.parametrize(
  "source",
  [
    "int a<:2:>;",
    "<% int a; %>",
    "%:define X 1\n",
    "%:%:",
    "<::>",
    "<:::X",
    "#include_next <a/b>\n",
    "#import <a/b>\n",
    "#if __has_include_next(<a/b>)\n#endif\n",
    "#define H <a/b>\n#include H\n",
    "#define H <a/b>\n#if __has_include(H)\n#endif\n",
    "#embed <a/b>\n",
    "#if __has_embed(<a/b>)\n#endif\n",
    "import <a/b>;",
    'export import "a.hpp";',
    "#using <a/b>\n",
    "#unknown X\n",
    '#123 "a.hpp"\n',
    "#define F(x) #x\n",
    "#define F(x) x##x\n",
    r"int \u0061;",
    r"int \U00000061;",
  ],
)
def test_cpp_token_digest_rejects_unsupported_preprocessing_shapes(source):
  with pytest.raises(ValueError, match="Unsupported"):
    cpp_token_digest(source)


@pytest.mark.parametrize(
  "macro",
  [
    "__LINE__",
    "__FILE__",
    "__COUNTER__",
    "__DATE__",
    "__TIME__",
    "__builtin_LINE",
    "__builtin_COLUMN",
    "__builtin_source_location",
    "source_location",
  ],
)
def test_cpp_token_digest_rejects_layout_or_environment_sensitive_macros(macro):
  with pytest.raises(ValueError, match="location-sensitive"):
    cpp_token_digest(f"auto value = {macro};")
  assert cpp_token_digest(f'auto value = "{macro}";')


@pytest.mark.parametrize(
  ("algorithm", "version"),
  [
    ("unknown", CPP_TOKEN_VERSION),
    (None, CPP_TOKEN_VERSION),
    (CPP_TOKEN_ALGORITHM, None),
    (CPP_TOKEN_ALGORITHM, 0),
    (CPP_TOKEN_ALGORITHM, 2),
    (CPP_TOKEN_ALGORITHM, True),
  ],
)
def test_cpp_token_digest_rejects_unknown_or_missing_algorithm_versions(algorithm, version):
  with pytest.raises(ValueError, match="Unsupported C\\+\\+ token digest"):
    cpp_token_digest("int a;", algorithm, version)
