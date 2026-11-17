const LY = require('../www/liuyao.js');
const assert = require('assert');

const CANON = [
  ['乾为天','天风姤','天山遁','天地否','风地观','山地剥','火地晋','火天大有'],
  ['坎为水','水泽节','水雷屯','水火既济','泽火革','雷火丰','地火明夷','地水师'],
  ['艮为山','山火贲','山天大畜','山泽损','火泽睽','天泽履','风泽中孚','风山渐'],
  ['震为雷','雷地豫','雷水解','雷风恒','地风升','水风井','泽风大过','泽雷随'],
  ['巽为风','风天小畜','风火家人','风雷益','天雷无妄','火雷噬嗑','山雷颐','山风蛊'],
  ['离为火','火山旅','火风鼎','火水未济','山水蒙','风水涣','天水讼','天火同人'],
  ['坤为地','地雷复','地泽临','地天泰','雷天大壮','泽天夬','水天需','水地比'],
  ['兑为泽','泽水困','泽地萃','泽山咸','水山蹇','地山谦','雷山小过','雷泽归妹']
];
const sortSeq = a => [...a].sort((x,y)=>x[0].localeCompare(y[0],'zh'));
assert.deepStrictEqual(sortSeq(LY.PALACE_SEQ), sortSeq(CANON), '八宫卦序');
const all = new Set();
LY.PALACE_SEQ.forEach(s => s.forEach(n => all.add(n)));
assert.strictEqual(all.size, 64, '64卦不重不漏');
assert.strictEqual(Object.keys(LY.TUAN).length, 64, '卦辞齐全');

// 乾为天 甲子日（日干甲0，日序0）
const qian = LY.chart(LY.bitsToLines([1,1,1,1,1,1]), 0, 0);
assert.strictEqual(qian.name, '乾为天');
assert.strictEqual(qian.palace.label, '乾宫本位卦');
const expectQian = [
  ['子','子孙','青龙',false], ['寅','妻财','朱雀',false], ['辰','父母','勾陈',true],
  ['午','官鬼','螣蛇',false], ['申','兄弟','白虎',false], ['戌','父母','玄武',false]];
qian.rows.forEach((r,i)=>{
  assert.strictEqual(r.zhi, expectQian[i][0]);
  assert.strictEqual(r.rel, expectQian[i][1]);
  assert.strictEqual(r.spirit, expectQian[i][2]);
  assert.strictEqual(r.ying, expectQian[i][3]);
  assert.strictEqual(r.shi, i===5);
});
assert.strictEqual(qian.ying, 2);
assert.deepStrictEqual(qian.kong, ['戌','亥']);
assert.strictEqual(qian.rows[5].kong, true);
assert.strictEqual(qian.fu.length, 0);

// 坤为地 己巳日（日干己5，日序5）
const kun = LY.chart(LY.bitsToLines([0,0,0,0,0,0]), 5, 5);
assert.strictEqual(kun.palace.label, '坤宫本位卦');
assert.deepStrictEqual(kun.rows.map(r=>r.zhi), ['未','巳','卯','丑','亥','酉']);
assert.deepStrictEqual(kun.rows.map(r=>r.rel), ['兄弟','父母','官鬼','兄弟','妻财','子孙']);
assert.strictEqual(kun.rows[0].spirit, '螣蛇');
assert.deepStrictEqual(kun.kong, ['戌','亥']);

// 天风姤：缺妻财，伏神妻财寅木于二爻
const gou = LY.chart(LY.bitsToLines([0,1,1,1,1,1]), 0, 0);
assert.strictEqual(gou.name, '天风姤');
assert.strictEqual(gou.palace.label, '乾宫一世卦');
assert.strictEqual(gou.shi, 0); assert.strictEqual(gou.ying, 3);
assert.strictEqual(gou.fu.length, 1);
assert.deepStrictEqual([gou.fu[0].pos, gou.fu[0].rel, gou.fu[0].zhi], [1, '妻财', '寅']);

// 动爻变卦：乾为天 初爻动 -> 天风姤
const mv = LY.chart([{yang:true,moving:true,value:9},{yang:true,moving:false,value:7},{yang:true,moving:false,value:7},
  {yang:true,moving:false,value:7},{yang:true,moving:false,value:7},{yang:true,moving:false,value:7}], 2, 42);
assert.strictEqual(mv.changed.name, '天风姤');
assert.strictEqual(mv.rows[0].changed.zhi, '丑');
assert.strictEqual(mv.rows[0].changed.rel, '父母');
assert.strictEqual(mv.rows[0].spirit, '朱雀'); // 丙丁日起朱雀

// 硬币概率分布
const cnt = {6:0,7:0,8:0,9:0};
let seed = 42;
function rng(){ seed = (seed * 1103515245 + 12345) % 2147483648; return seed / 2147483648; }
for (let i=0;i<200000;i++) cnt[LY.tossCoins(rng).sum]++;
assert.ok(Math.abs(cnt[6]/200000 - 0.125) < 0.01, '老阴1/8');
assert.ok(Math.abs(cnt[7]/200000 - 0.375) < 0.01, '少阳3/8');
assert.ok(Math.abs(cnt[8]/200000 - 0.375) < 0.01, '少阴3/8');
assert.ok(Math.abs(cnt[9]/200000 - 0.125) < 0.01, '老阳1/8');

// 全64卦 × 10日干 排盘无异常
for (const name of all) {
  const e = LY.PALACE[name];
  for (let g=0; g<10; g++) {
    const bits = [];
    // reconstruct bits from name via NAMES lookup
    outer: for (let u=0;u<8;u++) for (let l=0;l<8;l++) if (LY.NAMES[u][l]===name) {
      bits.push(...LY.TRI[l].bits, ...LY.TRI[u].bits); break outer;
    }
    const c = LY.chart(LY.bitsToLines(bits), g, (g*7)%60);
    assert.strictEqual(c.rows.length, 6);
    assert.strictEqual(c.name, name);
    assert.ok(c.kong.length === 2);
  }
}
console.log('六爻引擎自检全部通过：八宫卦序/64卦/纳甲/六亲/六神/世应/旬空/伏神/变卦/硬币分布');