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

import time
import shlex
import asyncio

from typing import Sequence, NamedTuple, Callable, Tuple


def green_print(cmd: str, end: str = '\n') -> None:
  '''Print text in green color.'''
  print(f'\033[92m{cmd}\033[0m', end=end)

def red_print(cmd: str, end: str = '\n') -> None:
  '''Print text in red color.'''
  print(f'\033[91m{cmd}\033[0m', end=end)

def yellow_print(cmd: str, end: str = '\n') -> None:
  '''Print text in yellow color.'''
  print(f'\033[93m{cmd}\033[0m', end=end)

def blue_print(cmd: str, end: str = '\n') -> None:
  '''Print text in blue color.'''
  print(f'\033[94m{cmd}\033[0m', end=end)


class ProcReturn(NamedTuple):
  '''A named tuple to store the return code and output of a process.'''
  retcode: int
  stdout: str
  stderr: str


async def stream_subprocess(cmd: Sequence[str], print_stdout: bool, print_stderr: bool, **kwargs) -> ProcReturn:
  '''Run a subprocess asynchronously and capture its output.'''
  process = await asyncio.create_subprocess_exec(
    *cmd,
    stdout=asyncio.subprocess.PIPE,
    stderr=asyncio.subprocess.PIPE,
    **kwargs
  )
  stdout_lines, stderr_lines = [], []

  async def read_stream(stream, buffer, do_print):
    '''Read output from a stream and store it in a buffer.'''
    while True:
      line = await stream.readline()
      if line:
        line_decoded = line.decode('utf-8', errors='replace')
        if do_print:
          print(line_decoded, end='')
        buffer.append(line_decoded)
      else:
        break

  await asyncio.wait([
    asyncio.create_task(read_stream(process.stdout, stdout_lines, print_stdout)),
    asyncio.create_task(read_stream(process.stderr, stderr_lines, print_stderr))
  ])

  retcode = await process.wait()
  return ProcReturn(retcode, ''.join(stdout_lines), ''.join(stderr_lines))


def run_cmd(
  cmd: Sequence[str],
  print_cmd: bool = True,
  print_stdout: bool = True,
  print_stderr: bool = True,
  **kwargs
) -> ProcReturn:
  '''Run a command synchronously and capture its output.'''
  if print_cmd:
    blue_print(f'# Command: {shlex.join(cmd)}')

  result = asyncio.run(stream_subprocess(cmd, print_stdout, print_stderr, **kwargs))

  if print_cmd:
    if result.retcode != 0:
      red_print(f'# Command failed:\n# {shlex.join(cmd)}')
    else:
      green_print(f'# Command succeeded:\n# {shlex.join(cmd)}')

  return result


def time_execution(func: Callable[[], int], task_name: str) -> Tuple[int, float]:
  '''Measure and print the execution time of a function.'''
  start_time = time.time()
  retcode = func()
  end_time = time.time()
  duration = end_time - start_time
  
  blue_print(f'# {task_name} time: {duration:.2f} seconds')
  return retcode, duration
