import asyncio
import shlex
import time
from typing import Sequence, NamedTuple, Callable, Tuple


def green_print(cmd: str, end: str = '\n') -> None:
  print(f'\033[92m{cmd}\033[0m', end=end)

def red_print(cmd: str, end: str = '\n') -> None:
  print(f'\033[91m{cmd}\033[0m', end=end)

def yellow_print(cmd: str, end: str = '\n') -> None:
  print(f'\033[93m{cmd}\033[0m', end=end)

def blue_print(cmd: str, end: str = '\n') -> None:
  print(f'\033[94m{cmd}\033[0m', end=end)


class ProcReturn(NamedTuple):
  retcode: int
  stdout: str
  stderr: str


async def stream_subprocess(cmd: Sequence[str], print_stdout: bool, print_stderr: bool, **kwargs) -> ProcReturn:
  process = await asyncio.create_subprocess_exec(
    *cmd,
    stdout=asyncio.subprocess.PIPE,
    stderr=asyncio.subprocess.PIPE,
    **kwargs
  )
  stdout_lines, stderr_lines = [], []

  async def read_stream(stream, buffer, print_func, should_print):
    while True:
      line = await stream.readline()
      if line:
        line_decoded = line.decode('utf-8')
        if should_print:
          print_func(line_decoded, end='')
        buffer.append(line_decoded)
      else:
        break

  await asyncio.wait([
    asyncio.create_task(read_stream(process.stdout, stdout_lines, print, print_stdout)),
    asyncio.create_task(read_stream(process.stderr, stderr_lines, print, print_stderr))
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
  if print_cmd:
    blue_print(f'# Command: {shlex.join(cmd)}')
  
  loop = asyncio.get_event_loop()
  result = loop.run_until_complete(stream_subprocess(cmd, print_stdout, print_stderr, **kwargs))

  if print_cmd:
    if result.retcode != 0:
      red_print(f'# Command failed:\n# {shlex.join(cmd)}')
    else:
      green_print(f'# Command succeeded:\n# {shlex.join(cmd)}')

  return result


def time_execution(func: Callable[[], int], task_name: str) -> Tuple[int, float]:
  start_time = time.time()
  retcode = func()
  end_time = time.time()
  duration = end_time - start_time
  blue_print(f'# {task_name} time: {duration:.2f} seconds')
  return retcode, duration
