/* 六爻纳甲排盘引擎：八宫、世应、纳甲、六亲、六神、旬空、伏神、变卦 */
(function (root) {
  'use strict';
  var ZHI = '子丑寅卯辰巳午未申酉戌亥';
  var WX_Z = '水土木木土火火土金金土水';
  var ELEM = ['木', '火', '土', '金', '水'];
  var REL = ['兄弟', '子孙', '父母', '妻财', '官鬼'];
  var SPIRITS = ['青龙', '朱雀', '勾陈', '螣蛇', '白虎', '玄武'];
  var SPIRIT_START = [0, 0, 1, 1, 2, 3, 4, 4, 5, 5];
  var POS_NAME = ['初爻', '二爻', '三爻', '四爻', '五爻', '上爻'];
  var STAGE = ['本位卦', '一世卦', '二世卦', '三世卦', '四世卦', '五世卦', '游魂卦', '归魂卦'];

  var TRI = [
    { name: '乾', sym: '☰', nature: '天', elem: 3, bits: [1, 1, 1], najia: ['子', '寅', '辰', '午', '申', '戌'] },
    { name: '兑', sym: '☱', nature: '泽', elem: 3, bits: [1, 1, 0], najia: ['巳', '卯', '丑', '亥', '酉', '未'] },
    { name: '离', sym: '☲', nature: '火', elem: 1, bits: [1, 0, 1], najia: ['卯', '丑', '亥', '酉', '未', '巳'] },
    { name: '震', sym: '☳', nature: '雷', elem: 0, bits: [1, 0, 0], najia: ['子', '寅', '辰', '午', '申', '戌'] },
    { name: '巽', sym: '☴', nature: '风', elem: 0, bits: [0, 1, 1], najia: ['丑', '亥', '酉', '未', '巳', '卯'] },
    { name: '坎', sym: '☵', nature: '水', elem: 4, bits: [0, 1, 0], najia: ['寅', '辰', '午', '申', '戌', '子'] },
    { name: '艮', sym: '☶', nature: '山', elem: 2, bits: [0, 0, 1], najia: ['辰', '午', '申', '戌', '子', '寅'] },
    { name: '坤', sym: '☷', nature: '地', elem: 2, bits: [0, 0, 0], najia: ['未', '巳', '卯', '丑', '亥', '酉'] }
  ];
  var TRI_BY_BITS = {};
  TRI.forEach(function (t, i) { TRI_BY_BITS[t.bits.join('')] = i; });

  var NAMES = [
    ['乾为天', '天泽履', '天火同人', '天雷无妄', '天风姤', '天水讼', '天山遁', '天地否'],
    ['泽天夬', '兑为泽', '泽火革', '泽雷随', '泽风大过', '泽水困', '泽山咸', '泽地萃'],
    ['火天大有', '火泽睽', '离为火', '火雷噬嗑', '火风鼎', '火水未济', '火山旅', '火地晋'],
    ['雷天大壮', '雷泽归妹', '雷火丰', '震为雷', '雷风恒', '雷水解', '雷山小过', '雷地豫'],
    ['风天小畜', '风泽中孚', '风火家人', '风雷益', '巽为风', '风水涣', '风山渐', '风地观'],
    ['水天需', '水泽节', '水火既济', '水雷屯', '水风井', '坎为水', '水山蹇', '水地比'],
    ['山天大畜', '山泽损', '山火贲', '山雷颐', '山风蛊', '山水蒙', '艮为山', '山地剥'],
    ['地天泰', '地泽临', '地火明夷', '地雷复', '地风升', '地水师', '地山谦', '坤为地']
  ];
  var TUAN = {
    '乾为天': '元亨，利贞。', '坤为地': '元亨，利牝马之贞。君子有攸往，先迷后得主，利。西南得朋，东北丧朋。安贞吉。',
    '水雷屯': '元亨利贞，勿用有攸往，利建侯。', '山水蒙': '亨。匪我求童蒙，童蒙求我。初筮告，再三渎，渎则不告。利贞。',
    '水天需': '有孚，光亨，贞吉。利涉大川。', '天水讼': '有孚窒惕，中吉，终凶。利见大人，不利涉大川。',
    '地水师': '贞，丈人吉，无咎。', '水地比': '吉。原筮，元永贞，无咎。不宁方来，后夫凶。',
    '风天小畜': '亨。密云不雨，自我西郊。', '天泽履': '履虎尾，不咥人，亨。',
    '地天泰': '小往大来，吉亨。', '天地否': '否之匪人，不利君子贞，大往小来。',
    '天火同人': '同人于野，亨。利涉大川，利君子贞。', '火天大有': '元亨。',
    '地山谦': '亨，君子有终。', '雷地豫': '利建侯行师。',
    '泽雷随': '元亨利贞，无咎。', '山风蛊': '元亨，利涉大川。先甲三日，后甲三日。',
    '地泽临': '元亨利贞。至于八月有凶。', '风地观': '盥而不荐，有孚颙若。',
    '火雷噬嗑': '亨。利用狱。', '山火贲': '亨。小利有攸往。',
    '山地剥': '不利有攸往。', '地雷复': '亨。出入无疾，朋来无咎。反复其道，七日来复，利有攸往。',
    '天雷无妄': '元亨利贞。其匪正有眚，不利有攸往。', '山天大畜': '利贞，不家食吉，利涉大川。',
    '山雷颐': '贞吉。观颐，自求口实。', '泽风大过': '栋桡，利有攸往，亨。',
    '坎为水': '习坎，有孚，维心亨，行有尚。', '离为火': '利贞，亨。畜牝牛，吉。',
    '泽山咸': '亨，利贞，取女吉。', '雷风恒': '亨，无咎，利贞，利有攸往。',
    '天山遁': '亨，小利贞。', '雷天大壮': '利贞。',
    '火地晋': '康侯用锡马蕃庶，昼日三接。', '地火明夷': '利艰贞。',
    '风火家人': '利女贞。', '火泽睽': '小事吉。',
    '水山蹇': '利西南，不利东北。利见大人，贞吉。', '雷水解': '利西南。无所往，其来复吉。有攸往，夙吉。',
    '山泽损': '有孚，元吉，无咎，可贞，利有攸往。曷之用？二簋可用享。', '风雷益': '利有攸往，利涉大川。',
    '泽天夬': '扬于王庭，孚号有厉。告自邑，不利即戎，利有攸往。', '天风姤': '女壮，勿用取女。',
    '泽地萃': '亨。王假有庙，利见大人，亨，利贞。用大牲吉，利有攸往。', '地风升': '元亨，用见大人，勿恤，南征吉。',
    '泽水困': '亨，贞，大人吉，无咎。有言不信。', '水风井': '改邑不改井，无丧无得，往来井井。汔至亦未繘井，羸其瓶，凶。',
    '泽火革': '巳日乃孚，元亨利贞，悔亡。', '火风鼎': '元吉，亨。',
    '震为雷': '亨。震来虩虩，笑言哑哑。震惊百里，不丧匕鬯。', '艮为山': '艮其背，不获其身，行其庭，不见其人，无咎。',
    '风山渐': '女归吉，利贞。', '雷泽归妹': '征凶，无攸利。',
    '雷火丰': '亨，王假之，勿忧，宜日中。', '火山旅': '亨，旅贞吉。',
    '巽为风': '小亨，利有攸往，利见大人。', '兑为泽': '亨，利贞。',
    '风水涣': '亨。王假有庙，利涉大川，利贞。', '水泽节': '亨，苦节不可贞。',
    '风泽中孚': '豚鱼吉，利涉大川，利贞。', '雷山小过': '亨，利贞。可小事，不可大事。飞鸟遗之音，不宜上，宜下，大吉。',
    '水火既济': '亨小，利贞。初吉终乱。', '火水未济': '亨。小狐汔济，濡其尾，无攸利。'
  };

  function sheng(e) { return (e + 1) % 5; }
  function ke(e) { return (e + 2) % 5; }
  function relation(palaceElem, lineElem) {
    if (lineElem === palaceElem) return 0;
    if (sheng(palaceElem) === lineElem) return 1;
    if (sheng(lineElem) === palaceElem) return 2;
    if (ke(palaceElem) === lineElem) return 3;
    return 4;
  }
  function triOf(lines, from) { return TRI_BY_BITS[lines.slice(from, from + 3).map(function (l) { return l.yang ? 1 : 0; }).join('')]; }
  function hexName(bits) {
    var lo = TRI_BY_BITS[bits.slice(0, 3).join('')], up = TRI_BY_BITS[bits.slice(3, 6).join('')];
    return NAMES[up][lo];
  }
  function bitsToLines(bits) { return bits.map(function (b) { return { yang: !!b, moving: false, value: b ? 7 : 8 }; }); }

  var PALACE = {};
  var PALACE_SEQ = [];
  (function build() {
    for (var p = 0; p < 8; p++) {
      var bits = TRI[p].bits.concat(TRI[p].bits);
      var seq = [];
      var shiList = [5, 0, 1, 2, 3, 4, 3, 2];
      var flips = [[], [0], [1], [2], [3], [4], [3], [0, 1, 2]];
      var cur = bits.slice();
      for (var s = 0; s < 8; s++) {
        flips[s].forEach(function (i) { cur[i] = cur[i] ? 0 : 1; });
        var name = hexName(cur);
        var entry = { palace: p, stage: s, shi: shiList[s], ying: (shiList[s] + 3) % 6, name: name };
        PALACE[name] = entry;
        seq.push(name);
      }
      PALACE_SEQ.push(seq);
    }
  })();

  function najiaBranch(triIdx, lineIdx) { return TRI[triIdx].najia[lineIdx]; }

  function chart(lines, dayGan, dayIdx) {
    var bits = lines.map(function (l) { return l.yang ? 1 : 0; });
    var lo = triOf(lines, 0), up = triOf(lines, 3);
    var name = NAMES[up][lo];
    var pal = PALACE[name];
    var pe = TRI[pal.palace].elem;

    var changedBits = bits.map(function (b, i) { return lines[i].moving ? (b ? 0 : 1) : b; });
    var cLo = TRI_BY_BITS[changedBits.slice(0, 3).join('')], cUp = TRI_BY_BITS[changedBits.slice(3, 6).join('')];
    var changedName = NAMES[cUp][cLo];

    var xs = ((dayIdx % 12) - (dayIdx % 10) + 12) % 12;
    var kong = [(xs + 10) % 12, (xs + 11) % 12];
    var spStart = SPIRIT_START[dayGan];

    var rows = [];
    var present = [false, false, false, false, false];
    for (var i = 0; i < 6; i++) {
      var tri = i < 3 ? lo : up;
      var zhiChar = najiaBranch(tri, i);
      var zhiIdx = ZHI.indexOf(zhiChar);
      var elem = ELEM.indexOf(WX_Z[zhiIdx]);
      var rel = relation(pe, elem);
      present[rel] = true;
      var cTri = i < 3 ? cLo : cUp;
      var cZhi = najiaBranch(cTri, i);
      var cElem = ELEM.indexOf(WX_Z[ZHI.indexOf(cZhi)]);
      rows.push({
        pos: i, posName: POS_NAME[i],
        yang: !!bits[i], moving: !!lines[i].moving, value: lines[i].value,
        zhi: zhiChar, zhiIdx: zhiIdx, elem: ELEM[elem], rel: REL[rel], relIdx: rel,
        spirit: SPIRITS[(spStart + i) % 6],
        shi: i === pal.shi, ying: i === pal.ying,
        kong: kong.indexOf(zhiIdx) >= 0,
        changed: lines[i].moving ? { zhi: cZhi, elem: ELEM[cElem], rel: REL[relation(pe, cElem)], yang: !!changedBits[i] } : null
      });
    }
    var fu = [];
    var pureBits = TRI[pal.palace].bits.concat(TRI[pal.palace].bits);
    for (var r = 0; r < 5; r++) {
      if (!present[r]) {
        for (var j = 0; j < 6; j++) {
          var pz = najiaBranch(pal.palace, j);
          var pElem = ELEM.indexOf(WX_Z[ZHI.indexOf(pz)]);
          if (relation(pe, pElem) === r) {
            fu.push({ pos: j, rel: REL[r], zhi: pz, elem: ELEM[pElem] });
            break;
          }
        }
      }
    }
    return {
      name: name, tuan: TUAN[name] || '',
      upper: TRI[up], lower: TRI[lo],
      palace: { idx: pal.palace, name: TRI[pal.palace].name, elem: ELEM[pe], stage: STAGE[pal.stage],
        label: TRI[pal.palace].name + '宫' + STAGE[pal.stage] },
      shi: pal.shi, ying: pal.ying,
      rows: rows, fu: fu, kong: kong.map(function (k) { return ZHI[k]; }),
      changed: { name: changedName, tuan: TUAN[changedName] || '', bits: changedBits,
        palace: PALACE[changedName] ? TRI[PALACE[changedName].palace].name + '宫' : '',
        hasMoving: lines.some(function (l) { return l.moving; }) },
      dayGan: dayGan, dayIdx: dayIdx
    };
  }

  function tossCoins(rng) {
    var r = rng || Math.random;
    var coins = [0, 0, 0].map(function () { return r() < 0.5 ? 2 : 3; });
    var sum = coins[0] + coins[1] + coins[2];
    return { coins: coins, sum: sum, yang: sum === 7 || sum === 9, moving: sum === 6 || sum === 9 };
  }
  function castLines(rng) {
    var out = [];
    for (var i = 0; i < 6; i++) {
      var t = tossCoins(rng);
      out.push({ yang: t.yang, moving: t.moving, value: t.sum, coins: t.coins });
    }
    return out;
  }

  var api = { TRI: TRI, NAMES: NAMES, TUAN: TUAN, REL: REL, SPIRITS: SPIRITS, ELEM: ELEM, ZHI: ZHI,
    PALACE: PALACE, PALACE_SEQ: PALACE_SEQ, chart: chart, castLines: castLines, tossCoins: tossCoins,
    bitsToLines: bitsToLines, hexName: hexName, relation: relation };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.LY = api;
})(typeof self !== 'undefined' ? self : this);