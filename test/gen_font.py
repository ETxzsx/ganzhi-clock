# 生成 OLED 字库：16x16 中文子集（微软雅黑 Bold）+ 8x16 ASCII（Consolas）
# 列主序、每列 2 字节（低字节=上 8 行），与 ssd1306 绘制函数对应
import json
from PIL import Image, ImageFont

CJK = (
    "甲乙丙丁戊己庚辛壬癸子丑寅卯辰巳午未申酉戌亥"
    "正二三四五六七八九十冬腊月初廿"
    "小寒大立春雨水惊蛰春分清明谷雨立夏满芒种夏至暑处白露秋霜降冬雪"
    "日一二三四五六"
    "闰时校未配网络已连热点离线运行分秒起卦中连接成功失败静之"
    "干支钟启动离线运行联网校时农历生肖"
    "乾坤震巽坎离艮兑为天泽火雷风水山地履同人无妄姤讼遁否夬革随大过困咸萃"
    "有睽噬嗑鼎未济旅晋大壮归妹丰恒解小过豫小畜中孚家人益涣渐观需节既济屯井蹇比"
    "大畜损贲颐蛊蒙剥泰临明夷复升师谦"
    "父母妻财官鬼兄弟子孙青龙朱雀勾陈螣蛇白虎玄武木火土金水"
    "世应空伏神卦宫本位一世二三四五游魂归"
    "按键短长按切换界面起卦反色亮度状态休眠唤醒重试返回"
    "找到该密码错误或认证路由器拒绝接入信号不佳请"
    "年月柱旬变址版手机浏览器"
    "角亢氐房心尾箕井鬼柳星张翼轸奎娄胃昴毕觜参斗牛女虚危室壁"
    "玄枵纪析寿鹑首实沈梁降娵訾北南西东中将躔宿度距恒赤经式素昼夜置"
)
# 六十四卦全名（确保覆盖）
NAMES = [
    "乾为天 天泽履 天火同人 天雷无妄 天风姤 天水讼 天山遁 天地否",
    "泽天夬 兑为泽 泽火革 泽雷随 泽风大过 泽水困 泽山咸 泽地萃",
    "火天大有 火泽睽 离为火 火雷噬嗑 火风鼎 火水未济 火山旅 火地晋",
    "雷天大壮 雷泽归妹 雷火丰 震为雷 雷风恒 雷水解 雷山小过 雷地豫",
    "风天小畜 风泽中孚 风火家人 风雷益 巽为风 风水涣 风山渐 风地观",
    "水天需 水泽节 水火既济 水雷屯 水风井 坎为水 水山蹇 水地比",
    "山天大畜 山泽损 山火贲 山雷颐 山风蛊 山水蒙 艮为山 山地剥",
    "地天泰 地泽临 地火明夷 地雷复 地风升 地水师 地山谦 坤为地",
]
chars = set(CJK)
for row in NAMES:
    chars.update(row.replace(" ", ""))
chars = sorted(c for c in chars if ord(c) > 127)

f16 = ImageFont.truetype("C:/Windows/Fonts/msyhbd.ttc", 16)
f8 = ImageFont.truetype("C:/Windows/Fonts/consola.ttf", 14)


def bits16(ch, font, w, h):
    img = Image.new("L", (w, h), 0)
    from PIL import ImageDraw
    d = ImageDraw.Draw(img)
    bb = font.getbbox(ch)
    gw, gh = bb[2] - bb[0], bb[3] - bb[1]
    x = (w - gw) // 2 - bb[0]
    y = (h - gh) // 2 - bb[1]
    d.text((x, y), ch, font=font, fill=255)
    px = img.load()
    out = []
    for cx in range(w):
        lo = hi = 0
        for r in range(8):
            if px[cx, r] > 90:
                lo |= 1 << r
            if px[cx, r + 8] > 90:
                hi |= 1 << r
        out += [lo, hi]
    return out


lines = []
lines.append("/* 自动生成：test/gen_font.py（msyhbd 16px / consola 14px，阈值90） */")
lines.append("#pragma once")
lines.append("#include <stdint.h>")
lines.append("typedef struct { uint32_t cp; uint8_t b[32]; } gz_glyph16_t;")
lines.append("static const gz_glyph16_t GZ_FONT16[] = {")
for c in chars:
    b = bits16(c, f16, 16, 16)
    lines.append("  {0x%04X,{%s}}," % (ord(c), ",".join(str(v) for v in b)))
lines.append("};")
lines.append("static const uint16_t GZ_FONT16_N = %d;" % len(chars))
lines.append("static const uint8_t GZ_ASC8[95][16] = {")
for i in range(95):
    b = bits16(chr(32 + i), f8, 8, 16)
    # 8 宽：每列 2 字节共 16 字节
    lines.append("  {%s}," % ",".join(str(v) for v in b))
lines.append("};")
open("G:/AI data/esp32-ganzhi/main/gz_font.h", "w", encoding="utf-8").write("\n".join(lines) + "\n")
print("gz_font.h:", len(chars), "CJK glyphs")