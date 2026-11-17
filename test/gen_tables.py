# -*- coding: utf-8 -*-
import json, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
t = json.load(open('tables.json', encoding='utf-8'))

def arr(a, per=10):
    lines = []
    for i in range(0, len(a), per):
        lines.append(','.join(repr(x) for x in a[i:i+per]))
    return '[\n' + ',\n'.join(lines) + ']'

js = 'var XL0=' + arr(t['xl0']) + ';\n'
js += 'var NUT_B=' + arr(t['nut'], 5) + ';\n'
js += 'var DT_AT=' + arr(t['dt'], 8) + ';\n'
open('../www/astro_tables.js', 'w', encoding='utf-8').write(js)

from lunar_python import LunarYear
vals = []
for y in range(1900, 2101):
    ms = LunarYear.fromYear(y).getMonths()
    start = next(i for i, m in enumerate(ms) if m.getMonth() == 1)
    months = ms[start:]
    v = 0
    leap = 0
    for m in months:
        mm = m.getMonth()
        if mm < 0:
            leap = -mm
            if m.getDayCount() == 30:
                v |= 0x10000
        elif 1 <= mm <= 12:
            if m.getDayCount() == 30:
                v |= (0x10000 >> mm)
            if mm == 12:
                break
        else:
            break
    v |= leap
    vals.append(v)
lines = []
for i in range(0, len(vals), 10):
    lines.append(','.join('0x%05x' % v for v in vals[i:i+10]) + ',')
open('lunar_table.txt', 'w', encoding='utf-8').write('\n'.join(lines))
print('astro_tables.js bytes:', len(js))
print('lunar entries:', len(vals))
print('1933:', hex(vals[33]))