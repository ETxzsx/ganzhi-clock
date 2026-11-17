// 与 test/host_test.c 输出格式逐行对拍
const fs = require('fs');
const GZ = require('../www/core.js');
const LY = require('../www/liuyao.js');
const mode = process.argv[2] || 'gz';
const lines = fs.readFileSync(0, 'utf8').trim().split(/\r?\n/);
const out = [];
if (mode === 'ly') {
  for (const ln of lines) {
    const [dg, di, ...v] = ln.trim().split(/\s+/).map(Number);
    const L = v.map(x => ({ value: x, yang: x === 7 || x === 9, moving: x === 6 || x === 9 }));
    const c = LY.chart(L, dg, di);
    let s = `${c.name} ${c.changed.name} ${c.shi} ${c.ying} ${c.kong.join('')}`;
    for (const r of c.rows) s += ` ${r.zhi}/${r.rel}/${r.spirit}/${r.moving ? 1 : 0}`;
    s += ` ${c.palace.idx} ${c.palace.stage === undefined ? '' : ''}`;
    // stage index: STAGE 名称反查
    const STAGE = ['本位卦', '一世卦', '二世卦', '三世卦', '四世卦', '五世卦', '游魂卦', '归魂卦'];
    s = s.trimEnd() + ' ' + STAGE.indexOf(c.palace.stage);
    out.push(s);
  }
} else {
  for (const ln of lines) {
    const t = Number(ln.trim());
    const o = GZ.compute(t);
    out.push(`${o.year.gz} ${o.month.gz} ${o.day.gz} ${o.hour.gz}|${o.Y} ${o.M} ${o.D} ${o.dow}|${o.lunar.monthCn} ${o.lunar.dayCn}|${o.kong.join('')}|${['小寒','大寒','立春','雨水','惊蛰','春分','清明','谷雨','立夏','小满','芒种','夏至','小暑','大暑','立秋','处暑','白露','秋分','寒露','霜降','立冬','小雪','大雪','冬至'].indexOf(o.monthJie)} ${GZ.TERMS.indexOf(o.termCur.name)} ${o.jieqiDay}|${o.termNext.name} ${o.hBranch}`);
  }
}
process.stdout.write(out.join('\n') + '\n');