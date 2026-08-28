#!/usr/bin/env python3
#
# CelestialCalendar:
#   A C++23-style library that performs astronomical calculations and date conversions among various calendars,
#   including Gregorian, Lunar, and Chinese Ganzhi calendars.
#
# Copyright (C) 2026 Ningqi Wang (0xf3cd)
# Email: nq.maigre@gmail.com
# Repo : https://github.com/0xf3cd/celestial-calendar
#
# This project is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This project is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this project. If not, see <https://www.gnu.org/licenses/>.

import argparse
import hashlib

from dataclasses import dataclass
from pathlib import Path
from typing import Final, Sequence

if __package__:
  from .erfa_identity import ERFA_RAW_BASE, ERFA_VERSION
  from .sofa_identity import SOFA_ARCHIVE_URL
else:
  from erfa_identity import ERFA_RAW_BASE, ERFA_VERSION
  from sofa_identity import SOFA_ARCHIVE_URL


REPO_ROOT: Final[Path] = Path(__file__).resolve().parents[1]
ROOT_NOTICE: Final[Path] = REPO_ROOT / "THIRD_PARTY_NOTICES.txt"
EMSCRIPTEN_REVISION: Final[str] = "ce75e06884093bcefb86a6b8fd56a5d62a4cc245"
SEPARATOR: Final[bytes] = ("=" * 78 + "\n").encode()
PREAMBLE: Final[bytes] = b"""Third-Party Notices

The project license is in LICENSE. The components below retain their own
terms. This repository-wide bundle is included in every distribution;
each section says which build input it applies to.

"""


@dataclass(frozen=True)
class NoticeSource:
  title: str
  applicability: str
  path: Path
  upstream: str
  sha256: str
  marking: tuple[str, ...] = ()


NOTICE_SOURCES: Final[tuple[NoticeSource, ...]] = (
  NoticeSource(
    title="Emscripten 6.0.6 — LICENSE",
    applicability="the Emscripten 6.0.6 WebAssembly build and generated JavaScript glue",
    path=Path("third_party/emscripten/6.0.6/emscripten-LICENSE"),
    upstream=f"https://github.com/emscripten-core/emscripten/blob/{EMSCRIPTEN_REVISION}/LICENSE",
    sha256="620a78084fc7ca97c0b5dea9abf891f3ffcadfdbf305276f099c9c4e12fc1d86",
  ),
  NoticeSource(
    title="musl — COPYRIGHT",
    applicability="the musl C library supplied by the Emscripten 6.0.6 WebAssembly build",
    path=Path("third_party/emscripten/6.0.6/musl-COPYRIGHT"),
    upstream=(
      f"https://github.com/emscripten-core/emscripten/blob/{EMSCRIPTEN_REVISION}/system/lib/libc/musl/COPYRIGHT"
    ),
    sha256="b870108ec5e7790e9f9919064f1b9421d62d5f9b0e6c230c6adf7ea2da62e97b",
  ),
  NoticeSource(
    title="libc++ — LICENSE.TXT",
    applicability="the libc++ library supplied by the Emscripten 6.0.6 WebAssembly build",
    path=Path("third_party/emscripten/6.0.6/libcxx-LICENSE.TXT"),
    upstream=(
      f"https://github.com/emscripten-core/emscripten/blob/{EMSCRIPTEN_REVISION}/system/lib/libcxx/LICENSE.TXT"
    ),
    sha256="539dd7aed86e8a4f12cbdd0e6c50c189c7d74847e4fecc64ce2c6ee3a01da38b",
  ),
  NoticeSource(
    title="libc++abi — LICENSE.TXT",
    applicability="the libc++abi library supplied by the Emscripten 6.0.6 WebAssembly build",
    path=Path("third_party/emscripten/6.0.6/libcxxabi-LICENSE.TXT"),
    upstream=(
      f"https://github.com/emscripten-core/emscripten/blob/{EMSCRIPTEN_REVISION}/system/lib/libcxxabi/LICENSE.TXT"
    ),
    sha256="e2b35be49f7284a45b7baca8fc7b3ab7440e7902392b2528a457816b5bb2a15c",
  ),
  NoticeSource(
    title="libunwind — LICENSE.TXT",
    applicability="the libunwind support supplied by the Emscripten 6.0.6 WebAssembly build",
    path=Path("third_party/emscripten/6.0.6/libunwind-LICENSE.TXT"),
    upstream=(
      f"https://github.com/emscripten-core/emscripten/blob/{EMSCRIPTEN_REVISION}/system/lib/libunwind/LICENSE.TXT"
    ),
    sha256="b5efebcaca80879234098e52d1725e6d9eb8fb96a19fce625d39184b705f7b6d",
  ),
  NoticeSource(
    title="compiler-rt — LICENSE.TXT",
    applicability="the compiler-rt builtins supplied by the Emscripten 6.0.6 WebAssembly build",
    path=Path("third_party/emscripten/6.0.6/compiler-rt-LICENSE.TXT"),
    upstream=(
      f"https://github.com/emscripten-core/emscripten/blob/{EMSCRIPTEN_REVISION}/system/lib/compiler-rt/LICENSE.TXT"
    ),
    sha256="1a8f1058753f1ba890de984e48f0242a3a5c29a6a8f2ed9fd813f36985387e8d",
  ),
  NoticeSource(
    title="IAU SOFA issue 2023-10-11 — SOFA Software License",
    applicability="the lunar and nutation data derived from IAU SOFA issue 2023-10-11",
    path=Path("src/test/provenance/sofa/2023-10-11/doc/copyr.lis"),
    upstream=f"{SOFA_ARCHIVE_URL} (member sofa/20231011/c/doc/copyr.lis)",
    sha256="ffe5460c057a4765e6ca7cf30b50e9f1306e84640e8ec9c05566bbad2c96c994",
    marking=(
      "Derived-work statement: this project uses data derived from software provided by SOFA under license.",
      "This project does not itself constitute software provided by or endorsed by SOFA.",
      "Source comments identify whether each table is truncated or complete and describe any scaling or argument "
      "reordering applied; project routine names do not use iau or sofa prefixes.",
      "The SOFA user-replaceable DAT terms for the leap-second derivation remain in the vendored dat.c source file.",
    ),
  ),
  NoticeSource(
    title=f"ERFA v{ERFA_VERSION} — LICENSE",
    applicability="the runtime constants and coefficients derived from the pinned ERFA source files",
    path=Path(f"src/test/provenance/erfa/v{ERFA_VERSION}/LICENSE"),
    upstream=f"{ERFA_RAW_BASE}/LICENSE",
    sha256="b1858f9a263f22c438a455a32945da51a31a0ae25a21055da13bb7ed57cc3b51",
  ),
)


def assemble_notices(
  repo_root: Path = REPO_ROOT,
  sources: Sequence[NoticeSource] = NOTICE_SOURCES,
  preamble: bytes = PREAMBLE,
  separator: bytes = SEPARATOR,
) -> bytes:
  """Assemble the canonical notice from immutable, independently pinned inputs."""
  paths = [source.path for source in sources]
  if len(paths) != len(set(paths)):
    raise RuntimeError("third-party notice inputs must be unique")

  sections = [preamble]
  for source in sources:
    body = (repo_root / source.path).read_bytes()
    digest = hashlib.sha256(body).hexdigest()
    if digest != source.sha256:
      raise RuntimeError(f"third-party notice input hash mismatch for {source.path}: {digest}")

    heading_lines = (
      source.title,
      f"Applies to: {source.applicability}.",
      f"Source: {source.upstream}",
      f"Source-file SHA-256: {source.sha256}",
      *source.marking,
    )
    heading = ("\n".join(heading_lines) + "\n").encode()
    sections.append(separator + heading + separator + b"\n" + body.rstrip(b"\n") + b"\n\n")

  return b"".join(sections)


def main() -> None:
  parser = argparse.ArgumentParser(description="Assemble or verify THIRD_PARTY_NOTICES.txt")
  parser.add_argument("--write", action="store_true", help="write the canonical repository file")
  args = parser.parse_args()

  expected = assemble_notices()
  if args.write:
    ROOT_NOTICE.write_bytes(expected)
  elif not ROOT_NOTICE.is_file() or ROOT_NOTICE.read_bytes() != expected:
    raise RuntimeError("THIRD_PARTY_NOTICES.txt is not the canonical assembled output")
  print(f"{hashlib.sha256(expected).hexdigest()}  {ROOT_NOTICE.name}")


if __name__ == "__main__":
  main()
