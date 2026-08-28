#!/usr/bin/env python3
#
# Helper to download release assets (latest release) from GitHub.
#
#########################################################################################
#
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

import sys
import argparse
import re

from pathlib import Path
from typing import Final, Union

# Apply a workaround to import from the parent directory...
sys.path.append(str(Path(__file__).parent.parent))

from automation import red_print, yellow_print, green_print
from automation.github import GitHub
from toolbox.release_validation import LicenseValidation, validate_release_archives


STRICT_ARCHIVE_VERSION: Final[tuple[int, int, int]] = (0, 6, 0)
LEGACY_LICENSE_VERSIONS: Final[frozenset[tuple[int, int, int]]] = frozenset({(0, 6, 0), (0, 6, 1)})


def find_release(keyword: Union[str, int]) -> GitHub.Release:
  """Find a release by its ID or tag name."""
  releases = GitHub.list_releases()
  for release in releases:
    if release.id == keyword or release.tag_name == keyword:
      return release

  raise RuntimeError(f'Cannot find release with keyword "{keyword}"')


def get_latest_release() -> GitHub.Release:
  """Find the latest release."""
  releases = GitHub.list_releases()
  if len(releases) == 0:
    red_print("Cannot find any releases")
    raise RuntimeError("Cannot find any releases")

  sorted_releases = sorted(releases, key=lambda release: release.published_at, reverse=True)
  return sorted_releases[0]


def archive_validation_version(tag_name: str) -> str | None:
  """Return the package version when a release uses the v0.6+ archive contract."""
  match = re.fullmatch(r"v(\d+)\.(\d+)\.(\d+)", tag_name)
  if match is None:
    raise RuntimeError(f"Cannot determine the archive contract from release tag {tag_name!r}")
  components = tuple(int(part) for part in match.groups())
  return ".".join(match.groups()) if components >= STRICT_ARCHIVE_VERSION else None


def release_license_validation(version: str) -> LicenseValidation:
  """Select the LICENSE member contract without binding historical bytes to this checkout."""
  match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", version)
  if match is None:
    raise RuntimeError(f"Cannot determine the LICENSE contract from version {version!r}")
  components = tuple(int(part) for part in match.groups())
  return LicenseValidation.LEGACY if components in LEGACY_LICENSE_VERSIONS else LicenseValidation.MEMBERS


def parse_args() -> argparse.Namespace:
  """Parse the command line arguments."""
  parser = argparse.ArgumentParser(
    description="Download release assets (latest release) from GitHub.",
    epilog=(
      "Examples of usage:\n"
      "  To download the latest release assets:\n"
      "    ./release_downloader.py -s/--save-to /some/path\n\n"
    ),
  )
  parser.add_argument("-s", "--save-to", type=Path, required=True, help="The directory to save the release assets to.")
  parser.add_argument("-id", "--id", type=int, required=False, help="The ID of the release.")
  parser.add_argument("-p", "--parallel", type=int, default=4, help="Number of parallel downloads (default: 4).")
  parser.add_argument("-t", "--tag", type=str, required=False, help="The tag of the release.")
  return parser.parse_args()


def validate_args(args: argparse.Namespace) -> None:  # Exception raised on failure.
  """Validate the command line arguments."""
  # Validate the number of parallel downloads.
  if args.parallel < 1:
    red_print(f"Invalid number of parallel downloads: {args.parallel}")
    raise RuntimeError(f"Invalid number of parallel downloads: {args.parallel}")

  # Validate the directory path.
  if args.save_to.exists() and args.save_to.is_file():
    red_print(f"Directory path is not a directory: {args.save_to}")
    raise RuntimeError(f"Directory path is not a directory: {args.save_to}")

  if args.id is None and args.tag is None:
    yellow_print("Neither --id nor --tag is specified. Download from latest release.")

  if args.id is not None and args.tag is not None:
    red_print("Both --id and --tag are specified. Only one is allowed.")
    raise RuntimeError("Both --id and --tag are specified. Only one is allowed.")


def main() -> None:
  args = parse_args()
  validate_args(args)

  def get_release() -> GitHub.Release:
    if args.id is not None:
      return find_release(args.id)
    elif args.tag is not None:
      return find_release(args.tag)
    return get_latest_release()

  release = get_release()
  validation_version = archive_validation_version(release.tag_name)
  downloaded_assets = GitHub.download_release(release.id, args.save_to, args.parallel)
  if validation_version is not None:
    # A general downloader may target any tag; its local README describes the checkout,
    # not that release. The tag checkout's collector already verified the documented matrix.
    validate_release_archives(
      downloaded_assets,
      validation_version,
      check_documented_runtime=False,
      require_wheels=True,
      license_validation=release_license_validation(validation_version),
    )
  green_print(f"Downloaded {len(downloaded_assets)} assets to {args.save_to}")


if __name__ == "__main__":
  main()
