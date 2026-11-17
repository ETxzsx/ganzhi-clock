/* 干支历核心算法：四柱、纳音、生肖、农历、二十四节气
   太阳视黄经采用寿星天文历 VSOP87 截断级数（与 lunar_python 同源），节气时刻误差约1秒 */
(function (root) {
  'use strict';
  var T = (typeof module !== 'undefined' && module.exports && typeof require === 'function')
    ? require('./astro_tables.js')
    : { XL0: root.XL0, NUT_B: root.NUT_B, DT_AT: root.DT_AT };
  var GAN = '甲乙丙丁戊己庚辛壬癸';
  var ZHI = '子丑寅卯辰巳午未申酉戌亥';
  var WX_G = '木木火火土土金金水水';
  var WX_Z = '水土木木土火火土金金土水';
  var SX = '鼠牛虎兔龙蛇马羊猴鸡狗猪';
  var NAYIN = ['海中金', '炉中火', '大林木', '路旁土', '剑锋金', '山头火', '涧下水', '城头土', '白蜡金', '杨柳木',
    '泉中水', '屋上土', '霹雳火', '松柏木', '长流水', '沙中金', '山下火', '平地木', '壁上土', '金箔金',
    '覆灯火', '天河水', '大驿土', '钗钏金', '桑柘木', '大溪水', '沙中土', '天上火', '石榴木', '大海水'];
  var YUE = ['正月', '二月', '三月', '四月', '五月', '六月', '七月', '八月', '九月', '十月', '冬月', '腊月'];
  var RI = ['初一', '初二', '初三', '初四', '初五', '初六', '初七', '初八', '初九', '初十',
    '十一', '十二', '十三', '十四', '十五', '十六', '十七', '十八', '十九', '二十',
    '廿一', '廿二', '廿三', '廿四', '廿五', '廿六', '廿七', '廿八', '廿九', '三十'];
  var TERMS = ['小寒', '大寒', '立春', '雨水', '惊蛰', '春分', '清明', '谷雨', '立夏', '小满', '芒种', '夏至',
    '小暑', '大暑', '立秋', '处暑', '白露', '秋分', '寒露', '霜降', '立冬', '小雪', '大雪', '冬至'];
  var SHICHEN_RANGE = ['23:00–00:59', '01:00–02:59', '03:00–04:59', '05:00–06:59', '07:00–08:59', '09:00–10:59',
    '11:00–12:59', '13:00–14:59', '15:00–16:59', '17:00–18:59', '19:00–20:59', '21:00–22:59'];
  var WEEK = ['日', '一', '二', '三', '四', '五', '六'];
  var lunarInfo = [
    0x04bd8, 0x04ae0, 0x0a570, 0x054d5, 0x0d260, 0x0d950, 0x16554, 0x056a0, 0x09ad0, 0x055d2,
    0x04ae0, 0x0a5b6, 0x0a4d0, 0x0d250, 0x1d255, 0x0b540, 0x0d6a0, 0x0ada2, 0x095b0, 0x14977,
    0x04970, 0x0a4b0, 0x0b4b5, 0x06a50, 0x06d40, 0x1ab54, 0x02b60, 0x09570, 0x052f2, 0x04970,
    0x06566, 0x0d4a0, 0x0ea50, 0x16a95, 0x05ad0, 0x02b60, 0x186e3, 0x092e0, 0x1c8d7, 0x0c950,
    0x0d4a0, 0x1d8a6, 0x0b550, 0x056a0, 0x1a5b4, 0x025d0, 0x092d0, 0x0d2b2, 0x0a950, 0x0b557,
    0x06ca0, 0x0b550, 0x15355, 0x04da0, 0x0a5b0, 0x14573, 0x052b0, 0x0a9a8, 0x0e950, 0x06aa0,
    0x0aea6, 0x0ab50, 0x04b60, 0x0aae4, 0x0a570, 0x05260, 0x0f263, 0x0d950, 0x05b57, 0x056a0,
    0x096d0, 0x04dd5, 0x04ad0, 0x0a4d0, 0x0d4d4, 0x0d250, 0x0d558, 0x0b540, 0x0b6a0, 0x195a6,
    0x095b0, 0x049b0, 0x0a974, 0x0a4b0, 0x0b27a, 0x06a50, 0x06d40, 0x0af46, 0x0ab60, 0x09570,
    0x04af5, 0x04970, 0x064b0, 0x074a3, 0x0ea50, 0x06b58, 0x05ac0, 0x0ab60, 0x096d5, 0x092e0,
    0x0c960, 0x0d954, 0x0d4a0, 0x0da50, 0x07552, 0x056a0, 0x0abb7, 0x025d0, 0x092d0, 0x0cab5,
    0x0a950, 0x0b4a0, 0x0baa4, 0x0ad50, 0x055d9, 0x04ba0, 0x0a5b0, 0x15176, 0x052b0, 0x0a930,
    0x07954, 0x06aa0, 0x0ad50, 0x05b52, 0x04b60, 0x0a6e6, 0x0a4e0, 0x0d260, 0x0ea65, 0x0d530,
    0x05aa0, 0x076a3, 0x096d0, 0x04afb, 0x04ad0, 0x0a4d0, 0x1d0b6, 0x0d250, 0x0d520, 0x0dd45,
    0x0b5a0, 0x056d0, 0x055b2, 0x049b0, 0x0a577, 0x0a4b0, 0x0aa50, 0x1b255, 0x06d20, 0x0ada0,
    0x14b63, 0x09370, 0x049f8, 0x04970, 0x064b0, 0x168a6, 0x0ea50, 0x06b20, 0x1a6c4, 0x0aae0,
    0x092e0, 0x0d2e3, 0x0c960, 0x0d557, 0x0d4a0, 0x0da50, 0x05d55, 0x056a0, 0x0a6d0, 0x055d4,
    0x052d0, 0x0a9b8, 0x0a950, 0x0b4a0, 0x0b6a6, 0x0ad50, 0x055a0, 0x0aba4, 0x0a5b0, 0x052b0,
    0x0b273, 0x06930, 0x07337, 0x06aa0, 0x0ad50, 0x14b55, 0x04b60, 0x0a570, 0x054e4, 0x0d160,
    0x0e968, 0x0d520, 0x0daa0, 0x16aa6, 0x056d0, 0x04ae0, 0x0a9d4, 0x0a2d0, 0x0d150, 0x0f252,
    0x0d520];

  var SPR = 206264.80624709636;
  function rad(d) { return d * Math.PI / 180; }
  function norm360(d) { d %= 360; return d < 0 ? d + 360 : d; }
  function eLon(t, n) {
    t /= 10;
    var v = 0, tn = 1;
    var m0 = T.XL0[2] - T.XL0[1];
    for (var i = 0; i < 6; i++) {
      var n1 = T.XL0[1 + i], n2 = T.XL0[2 + i], n0 = n2 - n1;
      if (n0 === 0) continue;
      var m;
      if (n < 0) m = n2;
      else { m = Math.floor(3 * n * n0 / m0 + 0.5) + n1; if (i !== 0) m += 3; if (m > n2) m = n2; }
      var c = 0;
      for (var j = n1; j < m; j += 3) c += T.XL0[j] * Math.cos(T.XL0[j + 1] + t * T.XL0[j + 2]);
      v += c * tn; tn *= t;
    }
    v /= T.XL0[0];
    var t2 = t * t, t3 = t2 * t;
    v += (-0.0728 - 2.7702 * t - 1.1019 * t2 - 0.0996 * t3) / SPR;
    return v;
  }
  function nutationLon2(t) {
    var t2 = t * t, dl = 0;
    for (var i = 0; i < T.NUT_B.length; i += 5) {
      var a = i === 0 ? -1.742 * t : 0;
      dl += (T.NUT_B[i + 3] + a) * Math.sin(T.NUT_B[i] + T.NUT_B[i + 1] * t + T.NUT_B[i + 2] * t2);
    }
    return dl / 100 / SPR;
  }
  function gxcSunLon(t) {
    var v = -0.043126 + 628.301955 * t - 0.000002732 * t * t;
    var e = 0.016708634 - 0.000042037 * t - 0.0000001267 * t * t;
    return -20.49552 * (1 + e * Math.cos(v)) / SPR;
  }
  function saLon(t, n) { return eLon(t, n) + nutationLon2(t) + gxcSunLon(t) + Math.PI; }
  function ev(t) {
    var f = 628.307585 * t;
    return 628.332 + 21 * Math.sin(1.527 + f) + 0.44 * Math.sin(1.48 + f * 2)
      + 0.129 * Math.sin(5.82 + f) * t + 0.00055 * Math.sin(4.21 + f) * t * t;
  }
  function dtExt(y, jsd) { var dy = (y - 1820) / 100; return -20 + jsd * dy * dy; }
  function dtCalc(y) {
    var A = T.DT_AT, size = A.length, y0 = A[size - 2], t0 = A[size - 1];
    if (y >= y0) {
      var jsd = 31;
      if (y > y0 + 100) return dtExt(y, jsd);
      var v = dtExt(y, jsd), dv = dtExt(y0, jsd) - t0;
      return v - dv * (y0 + 100 - y) / 100;
    }
    var i = 0;
    for (i = 0; i < size; i += 5) { if (y < A[i + 5]) break; }
    var t1 = (y - A[i]) / (A[i + 5] - A[i]) * 10, t2 = t1 * t1, t3 = t2 * t1;
    return A[i + 1] + A[i + 2] * t1 + A[i + 3] * t2 + A[i + 4] * t3;
  }
  var termCache = {};
  function termMs(year, k) {
    var key = year + ':' + k;
    if (termCache[key] !== undefined) return termCache[key];
    var w = rad(norm360(285 + 15 * k));
    var t = ((year - 2000) * 365.2422 + 5.0 + k * 15.2184) / 36525;
    for (var i = 0; i < 32; i++) {
      var diff = w - saLon(t, -1);
      diff = Math.atan2(Math.sin(diff), Math.cos(diff));
      if (Math.abs(diff) < 1e-9) break;
      t += diff / ev(t);
    }
    var jde = 2451545 + t * 36525;
    var ms = (jde - 2440587.5) * 86400000 - dtCalc(year) * 1000;
    termCache[key] = ms;
    return ms;
  }
  function jdn(y, m, d) {
    var a = Math.floor((14 - m) / 12), y2 = y + 4800 - a, m2 = m + 12 * a - 3;
    return d + Math.floor((153 * m2 + 2) / 5) + 365 * y2 + Math.floor(y2 / 4)
      - Math.floor(y2 / 100) + Math.floor(y2 / 400) - 32045;
  }
  function gzIdx(stem, branch) { return ((6 * stem - 5 * branch) % 60 + 60) % 60; }
  function gzName(idx) { return GAN[idx % 10] + ZHI[idx % 12]; }

  function leapMonth(y) { return lunarInfo[y - 1900] & 0xf; }
  function leapDays(y) { return leapMonth(y) ? ((lunarInfo[y - 1900] & 0x10000) ? 30 : 29) : 0; }
  function monthDays(y, m) { return (lunarInfo[y - 1900] & (0x10000 >> m)) ? 30 : 29; }
  function lYearDays(y) {
    var s = 348;
    for (var i = 0x8000; i > 0x8; i >>= 1) s += (lunarInfo[y - 1900] & i) ? 1 : 0;
    return s + leapDays(y);
  }
  function solarToLunar(Y, M, D) {
    if (Y < 1900 || Y > 2100) return null;
    var offset = Math.round((Date.UTC(Y, M - 1, D) - Date.UTC(1900, 0, 31)) / 86400000);
    var ly = 1900, temp = 0;
    for (; ly < 2101 && offset > 0; ly++) { temp = lYearDays(ly); offset -= temp; }
    if (offset < 0) { offset += temp; ly--; }
    var leap = leapMonth(ly), isLeap = false, lm = 1;
    for (; lm < 13 && offset > 0; lm++) {
      if (leap > 0 && lm === leap + 1 && !isLeap) { --lm; isLeap = true; temp = leapDays(ly); }
      else temp = monthDays(ly, lm);
      if (isLeap && lm === leap + 1) isLeap = false;
      offset -= temp;
    }
    if (offset === 0 && leap > 0 && lm === leap + 1) {
      if (isLeap) isLeap = false; else { isLeap = true; --lm; }
    }
    if (offset < 0) { offset += temp; --lm; }
    return { year: ly, month: lm, day: offset + 1, leap: isLeap,
      monthCn: (isLeap ? '闰' : '') + YUE[lm - 1], dayCn: RI[offset] };
  }

  function compute(epochMs) {
    var t = epochMs;
    var bj = new Date(t + 8 * 3600000);
    var Y = bj.getUTCFullYear(), M = bj.getUTCMonth() + 1, D = bj.getUTCDate();
    var h = bj.getUTCHours(), mi = bj.getUTCMinutes(), s = bj.getUTCSeconds();
    var dow = bj.getUTCDay();

    var lichun = termMs(Y, 2);
    var gzYear = t < lichun ? Y - 1 : Y;
    var yIdx = ((gzYear - 4) % 60 + 60) % 60;
    var yStem = yIdx % 10;

    var jieK = [0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22];
    var jieOrder = [11, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    var best = -Infinity, mOrder = 10, bestK = 22;
    for (var i = 0; i < 12; i++) {
      var tm = termMs(Y, jieK[i]);
      if (tm <= t && tm > best) { best = tm; mOrder = jieOrder[i]; bestK = jieK[i]; }
    }
    if (best === -Infinity) { best = termMs(Y - 1, 22); mOrder = 10; bestK = 22; }
    var mStem = ((yStem % 5) * 2 + 2 + mOrder) % 10;
    var mBranch = (mOrder + 2) % 12;
    var mIdx = gzIdx(mStem, mBranch);

    var dIdx = (jdn(Y, M, D) + 49) % 60;
    var dStem = dIdx % 10;
    var hBranch = Math.floor(((h + 1) % 24) / 2);
    var hourDayStem = h === 23 ? (dStem + 1) % 10 : dStem;
    var hStem = ((hourDayStem % 5) * 2 + hBranch) % 10;
    var hIdx = gzIdx(hStem, hBranch);

    var allTerms = [{ k: 23, year: Y - 1, ms: termMs(Y - 1, 23) }];
    for (var k = 0; k < 24; k++) allTerms.push({ k: k, year: Y, ms: termMs(Y, k) });
    allTerms.push({ k: 0, year: Y + 1, ms: termMs(Y + 1, 0) });
    allTerms.push({ k: 1, year: Y + 1, ms: termMs(Y + 1, 1) });
    var cur = allTerms[0], next = allTerms[1];
    for (var j = 0; j < allTerms.length; j++) {
      if (allTerms[j].ms <= t) cur = allTerms[j];
      else { next = allTerms[j]; break; }
    }
    var lunar = solarToLunar(Y, M, D);
    var xs = ((dIdx % 12) - (dIdx % 10) + 12) % 12;
    var kong = [ZHI[(xs + 10) % 12], ZHI[(xs + 11) % 12]];

    function pillar(idx) {
      var g = idx % 10, z = idx % 12;
      return { idx: idx, gz: gzName(idx), gan: GAN[g], zhi: ZHI[z],
        wxG: WX_G[g], wxZ: WX_Z[z], nayin: NAYIN[Math.floor(idx / 2)] };
    }
    return {
      epoch: t, Y: Y, M: M, D: D, h: h, mi: mi, s: s, dow: dow, weekCn: WEEK[dow],
      year: pillar(yIdx), month: pillar(mIdx), day: pillar(dIdx), hour: pillar(hIdx),
      gzYear: gzYear, shengxiao: SX[yIdx % 12], kong: kong, monthJie: TERMS[bestK],
      shichen: ZHI[hBranch], shichenName: ZHI[hBranch] + '时', shichenRange: SHICHEN_RANGE[hBranch],
      hBranch: hBranch,
      lunar: lunar,
      termCur: { name: TERMS[cur.k], ms: cur.ms },
      termNext: { name: TERMS[next.k], ms: next.ms },
      jieqiDay: Math.floor((t - cur.ms) / 86400000) + 1
    };
  }

  var api = { compute: compute, GAN: GAN, ZHI: ZHI, TERMS: TERMS, NAYIN: NAYIN, WX_G: WX_G, WX_Z: WX_Z,
    SHICHEN_RANGE: SHICHEN_RANGE, gzName: gzName, gzIdx: gzIdx, termMs: termMs, solarToLunar: solarToLunar };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.GZ = api;
})(typeof self !== 'undefined' ? self : this);