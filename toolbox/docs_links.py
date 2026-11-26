# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions between
#   Gregorian and Chinese Lunar calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
# SPDX-License-Identifier: MIT

import posixpath
import re
from urllib.parse import quote, unquote, urlsplit


REPOSITORY = "https://github.com/0xf3cd/celestial-calendar"
HOSTED = "https://0xf3cd.github.io/celestial-calendar"


def repository_source(target: str) -> str | None:
  """The source named by a live repository URL; GitHub service/UI routes remain external."""
  url, repository = urlsplit(target), urlsplit(REPOSITORY)
  if (url.scheme, url.netloc) != (repository.scheme, repository.netloc):
    return None
  route = url.path.removeprefix(repository.path)
  if route == url.path:
    return None
  if route in ("", "/", "/tree/main", "/tree/main/", "/blob/main"):
    return "README.md"
  if route.startswith(("/blob/main/", "/tree/main/")):
    path = unquote(route.split("/", 3)[3])
    if not path:
      raise ValueError(f"repository source URL has no file: {target}")
    return path
  return None


def pinned_source(path: str, sha: str, directory: bool = False) -> str:
  """Name a repository-relative file or directory at an exact source commit."""
  if re.fullmatch(r"[0-9a-f]{40}", sha) is None:
    raise ValueError("documentation source SHA must be 40 lowercase hexadecimal characters")
  if path.startswith("/") or "\\" in path or "\0" in path or ".." in path.split("/"):
    raise ValueError(f"invalid repository source path: {path}")
  return f"{REPOSITORY}/{'tree' if directory else 'blob'}/{sha}/{quote(posixpath.normpath(path))}"


def pin_package_links(text: str, sha: str) -> str:
  """Replace this project's known literal documentation URLs without reserializing Markdown."""
  pinned_source("README.md", sha)
  for route in ("blob", "tree"):
    text = text.replace(f"{REPOSITORY}/{route}/main/", f"{REPOSITORY}/{route}/{sha}/")
  for language in ("python", "javascript"):
    source = f"bindings/{language}/README.md"
    text = text.replace(f"{HOSTED}/{source.removesuffix('.md')}.html", pinned_source(source, sha))
  return text
