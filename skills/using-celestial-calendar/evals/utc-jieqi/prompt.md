---
tags: [consumer, python, time-scales]
max_turns: 20
timeout_seconds: 600
allowed_tools: [Read, Glob, Grep, Skill, Write, Edit, Bash]
---

用已安装的 celestial_calendar 写 solution.py，接收一个带 Z 的 UTC 时间参数，
找严格晚于它的下一个节气，输出节气中文名、UTC 时刻和固定 UTC+8 的时刻。
时间输出保留小数秒，至少到毫秒。请运行 2026-12-31T15:23:45Z，并说明输入、
比较用的时标和显示偏移。只使用库已有的公开转换接口，不安装或升级包。

目录里的 docs/index.json 将公开文档 URL 映射到冻结的本地副本，请用它们而不联网。
runtime.json 说明已提供的运行环境。不要读取评分文件或答案。
