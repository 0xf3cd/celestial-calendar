#ifndef __COMMON_DEFINES_HPP__
#define __COMMON_DEFINES_HPP__

namespace common::defines {

enum class Jieqi {
  立春, 雨水, 惊蛰, 春分, 清明, 谷雨, 立夏, 小满, 芒种, 夏至, 小暑, 大暑,
  立秋, 处暑, 白露, 秋分, 寒露, 霜降, 立冬, 小雪, 大雪, 冬至, 小寒, 大寒,

  /* English aliases. */
  LICHUN = 立春, YUSHUI = 雨水, JINGZHE = 惊蛰, CHUNFEN = 春分, QINGMING = 清明, GUYU = 谷雨,
  LIXIA = 立夏, XIAOMAN = 小满, MANGZHONG = 芒种, XIAZHI = 夏至, XIAOSHU = 小暑, DASHU = 大暑,
  LIQIU = 立秋, CHUSHU = 处暑, BAILU = 白露, QIUFEN = 秋分, HANLU = 寒露, SHUANGJIANG = 霜降,
  LIDONG = 立冬, XIAOXUE = 小雪, DAXUE = 大雪, DONGZHI = 冬至, XIAOHAN = 小寒, DAHAN = 大寒,
};

} // namespace common::defines

#endif // __COMMON_DEFINES_HPP__
