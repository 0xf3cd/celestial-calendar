# CelestialCalendar Statistics:
#   Golden-dataset crawlers and evaluation notebooks for the CelestialCalendar C++ project.
#   No model training happens here (see AGENTS.md).
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar
# License: GNU General Public License v3.0
#
# This software is distributed without any warranty.
# See <https://www.gnu.org/licenses/> for more details.

# This file is used to download:
# - the Moon Phases from "丹尼爾的神祕學世界"（The Secret World of Daniel）
# Ref: https://www.taipeidaniel.idv.tw/articles-astrology-moon-new-full.htm
#
# Status (#168, 2026-08-08): the output `moon_phases.csv` has no test consumer, and no evidence
# of ever having had one -- it appears nowhere under `src/`. Kept as an evaluation asset for
# `new_moon.ipynb`; revisit if the source above stops being reachable or reproducible.
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar


from pathlib import Path

import requests
import pandas as pd
from bs4 import BeautifulSoup


URL = "https://www.taipeidaniel.idv.tw/articles-astrology-moon-new-full.htm"
CSV_PATH = Path(__file__).parent / "moon_phases.csv"


def main() -> None:
  response = requests.get(URL, timeout=30)
  response.raise_for_status()  # An error page parses into plausible-looking garbage otherwise.
  soup = BeautifulSoup(response.content, "html.parser")

  # Find all tables
  tables = soup.find_all("table")

  all_data = []
  for table in tables:
    rows = table.find_all("tr")
    data = []
    for row in rows:
      cols = row.find_all("td")
      cols = [ele.text.strip() for ele in cols]
      if cols:
        data.append(cols)
    if data:
      df = pd.DataFrame(data, columns=["日期", "時間", "狀態"])
      all_data.append(df)

  if not all_data:
    raise RuntimeError(f"No moon phase table found at {URL}")

  # Concatenate all dataframes
  combined_df = pd.concat(all_data, ignore_index=True)
  combined_df.to_csv(CSV_PATH, index=False, encoding="utf-8")
  print(f"All tables saved to {CSV_PATH}.")


if __name__ == "__main__":
  main()
