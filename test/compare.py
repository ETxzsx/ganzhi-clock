# -*- coding: utf-8 -*-
import sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

js = {}
js_term = {}
for line in open('js_out.txt', encoding='utf-8'):
    p = line.rstrip('\n').split('\t')
    if p[0] == 'TERM':
        js_term[(p[1], p[3])] = p[4]
    else:
        js[p[0]] = p[1:]

py = {}
py_term = {}
for line in open('py_out.txt', encoding='utf-8'):
    p = line.rstrip('\n').split('\t')
    if p[0] == 'TERM':
        name = '冬至' if p[2] == 'DONG_ZHI' else p[2]
        if p[3].startswith(p[1]):  # only terms within that calendar year
            py_term[(p[1], name)] = p[3]
    else:
        py[p[0]] = p[1:]

bad = 0
shown = 0
fields = ['年柱','月柱','日柱','时柱','农历','生肖','纳音','节气']
for k, v in py.items():
    jv = js.get(k)
    if jv is None:
        print('MISSING', k); bad += 1; continue
    for i, (a, b) in enumerate(zip(jv, v)):
        if a != b:
            bad += 1
            if shown < 30:
                print(f'DIFF {k} {fields[i]}: js={a} py={b}')
                shown += 1
print(f'日期样本: {len(py)}  字段差异: {bad}')

tbad = 0; tshown = 0; tmax = 0
from datetime import datetime
for k, v in py_term.items():
    jv = js_term.get(k)
    if jv is None:
        print('TERM MISSING', k); tbad += 1; continue
    d1 = datetime.strptime(jv, '%Y-%m-%d %H:%M')
    d2 = datetime.strptime(v, '%Y-%m-%d %H:%M')
    diff = abs((d1 - d2).total_seconds()) / 60
    tmax = max(tmax, diff)
    if diff > 2:
        tbad += 1
        if tshown < 20:
            print(f'TERM DIFF {k}: js={jv} py={v} ({diff:.0f}min)')
            tshown += 1
print(f'节气样本: {len(py_term)}  超2分钟差异: {tbad}  最大差: {tmax:.1f}分钟')