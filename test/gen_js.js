const GZ = require('../www/core.js');
const out = [];
function pad(n){return String(n).padStart(2,'0');}
for (let y = 1901; y <= 2099; y++) {
  for (let doy = 0; doy < 366; doy += 5) {
    const d = new Date(Date.UTC(y, 0, 1 + doy));
    if (d.getUTCFullYear() !== y) continue;
    const Y = d.getUTCFullYear(), M = d.getUTCMonth() + 1, D = d.getUTCDate();
    for (const h of [0, 6, 12, 23]) {
      const epoch = Date.UTC(Y, M - 1, D, h - 8, 30, 0);
      const r = GZ.compute(epoch);
      const lun = r.lunar;
      out.push([`${Y}-${pad(M)}-${pad(D)} ${pad(h)}:30`,
        r.year.gz, r.month.gz, r.day.gz, r.hour.gz,
        lun ? `${lun.year}|${lun.leap ? -lun.month : lun.month}|${lun.day}` : 'null',
        r.shengxiao, r.year.nayin, r.termCur.name].join('\t'));
    }
  }
}
// 节气时刻（北京时间）
for (let y = 1950; y <= 2080; y++) {
  for (let k = 0; k < 24; k++) {
    const ms = GZ.termMs(y, k);
    const bj = new Date(ms + 8 * 3600000);
    out.push(['TERM', y, k, GZ.TERMS[k],
      `${bj.getUTCFullYear()}-${pad(bj.getUTCMonth()+1)}-${pad(bj.getUTCDate())} ${pad(bj.getUTCHours())}:${pad(bj.getUTCMinutes())}`].join('\t'));
  }
}
console.log(out.join('\n'));