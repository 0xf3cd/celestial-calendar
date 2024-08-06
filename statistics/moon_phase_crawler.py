# This file is used to download:
# - the Moon Phases from "丹尼爾的神祕學世界"（The Secret World of Daniel）
# Ref: https://www.taipeidaniel.idv.tw/articles-astrology-moon-new-full.htm
#
# Author : Ningqi Wang (0xf3cd)
# Email  : nq.maigre@gmail.com
# Repo   : https://github.com/0xf3cd/celestial-calendar


import requests
import pandas as pd
from bs4 import BeautifulSoup


url = 'https://www.taipeidaniel.idv.tw/articles-astrology-moon-new-full.htm'
response = requests.get(url)
soup = BeautifulSoup(response.content, 'html.parser')

# Find all tables
tables = soup.find_all('table')

all_data = []
for table in tables:
  rows = table.find_all('tr')
  data = []
  for row in rows:
    cols = row.find_all('td')
    cols = [ele.text.strip() for ele in cols]
    if cols:
      data.append(cols)
  if data:
    df = pd.DataFrame(data, columns=['日期', '時間', '狀態'])
    all_data.append(df)

# Concatenate all dataframes
if all_data:
  combined_df = pd.concat(all_data, ignore_index=True)
  combined_df.to_csv('moon_phases.csv', index=False, encoding='utf-8')

print('All tables saved to moon_phases.csv.')
