// 生成 C 天文表头文件（寿星 VSOP87 截断级数 / 章动 / ΔT）
const fs = require('fs');
const path = require('path');
const t = require('./tables.json');
function arr(name, a) {
  let s = 'static const double ' + name + '[' + a.length + '] = {\n';
  for (let i = 0; i < a.length; i += 6) {
    s += '  ' + a.slice(i, i + 6).map(v => {
      let x = String(v);
      if (!/[.eE]/.test(x)) x += '.0';
      return x;
    }).join(',') + ',\n';
  }
  return s + '};\n';
}
let out = '/* 自动生成：test/gen_c_tables.js，源 test/tables.json（与 www/astro_tables.js 同源） */\n#pragma once\n';
out += arr('GZ_XL0', t.xl0);
out += arr('GZ_NUT', t.nut);
out += arr('GZ_DT', t.dt);
fs.writeFileSync(path.join(__dirname, '..', 'main', 'gz_tables.h'), out);
console.log('gz_tables.h written', out.length, 'bytes');