<!--
  CelestialCalendar:
    A C++23-style library that performs astronomical calculations and date conversions between
    Gregorian and Chinese Lunar calendars.

  Copyright (C) 2026 Ningqi Wang (0xf3cd)
  Email: nq.maigre@gmail.com
  Repo : https://github.com/0xf3cd/celestial-calendar

  SPDX-License-Identifier: MIT
-->

# celestial-calendar

[@0xf3cd/celestial](https://www.npmjs.com/package/@0xf3cd/celestial) 的 ESM 别名包，
转发根入口和 `/date` 的全部导出（包括 TypeScript 声明），并精确依赖相同版本的主包。
别名包不复制实现，也不包含 WebAssembly 模块。

[English guide](https://github.com/0xf3cd/celestial-calendar/blob/main/README_EN.md)

本文对应当前 `0.7.0` 源码，不表示此版本或别名包已经发布到 npm。
发布前请从完整源码[构建主包与别名包](https://github.com/0xf3cd/celestial-calendar#wasm)，安装同版本的两个 tarball。

## 安装与示例

确认所需版本已发布到 npm 后，在应用项目目录中安装：

```sh
npm install celestial-calendar
```

支持 Node 22 或更新版本。以下每个 JavaScript 代码块均可独立作为应用项目中的 `example.mjs`，
从该目录用 `node example.mjs` 运行。

### 今天的阴历日期

先取固定 UTC+8 下的公历日期，再转换为阴历；不要直接传入含 `fraction` 的民用时刻。

```js
import * as celestial from "celestial-calendar";
import { dateToCivilAtOffset } from "celestial-calendar/date";

const eastEight = dateToCivilAtOffset(new Date(), 480);
const { year, month, day } = eastEight;
const gregorian = { year, month, day };

await celestial.init();
console.log("UTC+8 公历日期：", gregorian);
console.log("阴历日期：", celestial.lunar.fromGregorian("algo3", gregorian));
```

### 节气与 UTC 日期字段

下面分别查询立春的 UT1 时刻与当前时钟的 UTC 民用字段；两个输出之间没有时标转换。
节气的 `momentUt1` 不是 UTC，也不是东八区钟表时间。

```js
import * as celestial from "celestial-calendar";
import { dateToCivilUtc } from "celestial-calendar/date";

await celestial.init();
console.log(celestial.jieqi.moment(2026, celestial.Jieqi.LICHUN));
console.log(dateToCivilUtc(new Date()));
```

## 与主包的关系

`/date` 不加载 WASM，也不需要初始化；它只桥接 UTC 或固定偏移下的民用字段，不做 UTC/UT1/TT 转换。
当两个包名解析到同一份主包安装时，它们共享导出引用和初始化状态；这不保证混用版本或任意依赖图下的去重。
API 契约、阴历算法的日期基准及浏览器用法见
[主包文档](https://github.com/0xf3cd/celestial-calendar/tree/main/bindings/javascript)。
PyPI 上的同名包是独立的 Python 发行包。

## License

This alias's project-authored material is licensed under MIT; see `LICENSE`.
For retained third-party material in the dependency, see `@0xf3cd/celestial/THIRD_PARTY_NOTICES.txt`
in the installed primary package. That inventory belongs to the dependency and is not bundled in this alias.
