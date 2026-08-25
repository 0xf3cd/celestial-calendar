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

import contextlib
import hashlib
import io
import sys
import tempfile
import zipfile
from pathlib import Path
from types import SimpleNamespace

import verify_bootstrap


def invoke(selector, resource, pyz, selected_pip):
  original = (
    verify_bootstrap.resources,
    verify_bootstrap._ensure_virtualenv,
    verify_bootstrap._parse_pip_constraint_for_virtualenv,
    sys.argv,
  )
  verify_bootstrap.resources = SimpleNamespace(VIRTUALENV=resource)
  verify_bootstrap._ensure_virtualenv = lambda _python_version: (pyz, "21.7.1")
  verify_bootstrap._parse_pip_constraint_for_virtualenv = lambda _selector: selected_pip
  sys.argv = [str(verify_bootstrap.__file__), str(selector)]
  try:
    output = io.StringIO()
    with contextlib.redirect_stdout(output):
      verify_bootstrap.main()
    return output.getvalue().strip()
  finally:
    (
      verify_bootstrap.resources,
      verify_bootstrap._ensure_virtualenv,
      verify_bootstrap._parse_pip_constraint_for_virtualenv,
      sys.argv,
    ) = original


def expect_error(expected, *args):
  try:
    invoke(*args)
  except RuntimeError as error:
    assert expected in str(error)
  else:
    raise AssertionError(f"expected RuntimeError containing {expected!r}")


def main():
  with tempfile.TemporaryDirectory() as temporary_directory:
    root = Path(temporary_directory)
    selector = root / "constraints-cibuildwheel.txt"
    lock = root / "requirements-cibuildwheel.txt"
    resource = root / "virtualenv.toml"
    pyz = root / "virtualenv-21.7.1.pyz"
    wheel_member = "virtualenv/seed/wheels/embed/pip-26.2-py3-none-any.whl"
    wheel_bytes = b"embedded pip 26.2 wheel"

    selector.write_text("-c requirements-cibuildwheel.txt\npip==26.2\n", encoding="utf-8")
    with zipfile.ZipFile(pyz, "w") as archive:
      archive.writestr(wheel_member, wheel_bytes)
    pyz_digest = hashlib.sha256(pyz.read_bytes()).hexdigest()
    wheel_digest = hashlib.sha256(wheel_bytes).hexdigest()
    resource.write_text(f'[default]\nversion = "21.7.1"\nsha256 = "{pyz_digest}"\n', encoding="utf-8")
    lock.write_text(f"pip==26.2 \\\n    --hash=sha256:{wheel_digest}\n", encoding="utf-8")

    assert invoke(selector, resource, pyz, "26.2") == (
      "PASS cibuildwheel bootstrap uses hash-locked embedded pip 26.2 from virtualenv-21.7.1.pyz"
    )
    expect_error("cannot read the pip pin", selector, resource, pyz, "embed")
    expect_error("pip 26.2.1 is not embedded", selector, resource, pyz, "26.2.1")

    resource.write_text('[default]\nversion = "21.7.1"\nsha256 = "' + "0" * 64 + '"\n', encoding="utf-8")
    expect_error("bootstrap virtualenv does not match", selector, resource, pyz, "26.2")
    resource.write_text(f'[default]\nversion = "21.7.1"\nsha256 = "{pyz_digest}"\n', encoding="utf-8")

    lock.write_text("platformdirs==4.11.0\n", encoding="utf-8")
    expect_error("pip 26.2 is not pinned", selector, resource, pyz, "26.2")
    lock.write_text("pip==26.2 \\\n    --hash=sha256:" + "0" * 64 + "\n", encoding="utf-8")
    expect_error("embedded pip 26.2 wheel is not hash-locked", selector, resource, pyz, "26.2")

  print("PASS cibuildwheel bootstrap verifier accepted a valid fixture and rejected invalid fixtures")


if __name__ == "__main__":
  main()
