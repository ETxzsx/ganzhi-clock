# -*- coding: utf-8 -*-
import sys, io, datetime
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
from lunar_python import Solar

GAN = "甲乙丙丁戊己庚辛壬癸"
ZHI = "子丑寅卯辰巳午未申酉戌亥"
NAYIN = ['海中金','炉中火','大林木','路旁土','剑锋金','山头火','涧下水','城头土','白蜡金','杨柳木',
'泉中水','屋上土','霹雳火','松柏木','长流水','沙中金','山下火','平地木','壁上土','金箔金',
'覆灯火','天河水','大驿土','钗钏金','桑柘木','大溪水','沙中土','天上火','石榴木','大海水']
def nayin(gz):
    g, z = GAN.index(gz[0]), ZHI.index(gz[1])
    idx = next(n for n in range(60) if n % 10 == g and n % 12 == z)
    return NAYIN[idx // 2]

def pad(n): return f"{n:02d}"
rows = []
for y in range(1901, 2100):
    for doy in range(0, 366, 5):
        d = datetime.date(y, 1, 1) + datetime.timedelta(days=doy)
        if d.year != y: continue
        for h in (0, 6, 12, 23):
            l = Solar.fromYmdHms(d.year, d.month, d.day, h, 30, 0).getLunar()
            rows.append("\t".join([
                f"{d.year}-{pad(d.month)}-{pad(d.day)} {pad(h)}:30",
                l.getYearInGanZhiExact(), l.getMonthInGanZhiExact(),
                l.getDayInGanZhi(), l.getTimeInGanZhi(),
                f"{l.getYear()}|{l.getMonth()}|{l.getDay()}",
                l.getYearShengXiaoExact(), nayin(l.getYearInGanZhiExact()), l.getPrevJieQi(False).getName()]))
for y in range(1950, 2081):
    l = Solar.fromYmd(y, 6, 1).getLunar()
    table = l.getJieQiTable()
    for name, sol in table.items():
        rows.append("\t".join(["TERM", str(y), name,
            f"{sol.getYear()}-{pad(sol.getMonth())}-{pad(sol.getDay())} {pad(sol.getHour())}:{pad(sol.getMinute())}"]))
print("\n".join(rows))