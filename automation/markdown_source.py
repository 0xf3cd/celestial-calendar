# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# SPDX-License-Identifier: MIT

"""The prose a Markdown guide renders, read from a CommonMark parse and never from the source text.

A link or a sentence moved into an HTML comment still satisfies a substring match on the source, and it is gone from
what a reader sees (#328, #350). The checks that name guide content read it through here; the
surfaces that stay on source text, like the adjacent retained-attribution records, are
byte-oriented provenance checks and do not.
"""

from copy import copy
import re

from markdown_it import MarkdownIt
from markdown_it.token import Token

# markdown-it with tables stands in for GitHub's GFM renderer.
MARKDOWN = MarkdownIt("commonmark").enable("table")
# Authored guides use comments and canonical lower-case explicit anchors.
COMMENTS = re.compile(r"\s*(?:<!--(?!-?>)(?:(?!--!?>).)*-->\s*)+", re.DOTALL)
ANCHOR = re.compile(r'<a id="([a-z0-9-]+)">')


def block_tokens(text: str, *, validate_link=None) -> list[Token]:
  """The original token tree, using the same parser and options as source checks."""
  if validate_link is None:
    return MARKDOWN.parse(text)
  parser = copy(MARKDOWN)
  parser.validateLink = validate_link
  return parser.parse(text)


def tokens(text: str) -> list[Token]:
  """The block tokens and, after each inline one, its children."""
  return [part for token in block_tokens(text) for part in (token, *(token.children or ()))]


def link_targets(parsed: list[Token]) -> list[str]:
  """Where the links and images point, percent-escaped as rendered."""
  return [
    token.attrs["href" if token.type == "link_open" else "src"]
    for token in parsed
    if token.type in ("link_open", "image")
  ]


def links(parsed: list[Token]) -> list[tuple[str, str]]:
  """Each link as its visible text and its target."""
  found, target, label = [], None, []
  for token in parsed:
    if token.type == "link_open":
      target, label = token.attrs["href"], []
    elif token.type == "link_close" and target is not None:
      found.append(("".join(label), target))
      target = None
    elif target is not None and token.type in ("text", "code_inline"):
      label.append(token.content)
  return found


def visible_text(parsed: list[Token]) -> str:
  """The text of paragraphs, headings and table cells, inline code included.

  Fences, link titles and comments are not text.
  """
  parts = []
  for token in parsed:
    if token.type == "inline":
      parts.append("\n")
    elif token.type in ("text", "code_inline"):
      parts.append(token.content)
    elif token.type in ("softbreak", "hardbreak"):
      parts.append(" ")
  return "".join(parts)


def section_text(text: str, title: str) -> str | None:
  """The visible text of the one `## title` section, including its subsections.

  None unless exactly one `##` heading has that title.
  """
  parsed = tokens(text)
  starts = [
    index
    for index, token in enumerate(parsed[:-1])
    if token.type == "heading_open" and token.tag == "h2" and parsed[index + 1].content == title
  ]
  if len(starts) != 1:
    return None
  body = next(index for index in range(starts[0], len(parsed)) if parsed[index].type == "heading_close") + 1
  end = next(
    (index for index in range(body, len(parsed)) if parsed[index].type == "heading_open" and parsed[index].tag == "h2"),
    len(parsed),
  )
  return visible_text(parsed[body:end])
