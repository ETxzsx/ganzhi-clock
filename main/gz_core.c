/* 干支历 + 六爻排盘 C 移植（离线运行核心，算法与 www/core.js、www/liuyao.js 逐行对应） */
#include "gz_core.h"
#include "gz_tables.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "esp_random.h"

static const char *GAN[10] = { "甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸" };
static const char *ZHI[12] = { "子", "丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥" };
static const char *YUE[12] = { "正月", "二月", "三月", "四月", "五月", "六月", "七月", "八月", "九月", "十月", "冬月", "腊月" };
static const char *RI[30] = {
    "初一", "初二", "初三", "初四", "初五", "初六", "初七", "初八", "初九", "初十",
    "十一", "十二", "十三", "十四", "十五", "十六", "十七", "十八", "十九", "二十",
    "廿一", "廿二", "廿三", "廿四", "廿五", "廿六", "廿七", "廿八", "廿九", "三十"
};
static const char *TERMS[24] = {
    "小寒", "大寒", "立春", "雨水", "惊蛰", "春分", "清明", "谷雨", "立夏", "小满", "芒种", "夏至",
    "小暑", "大暑", "立秋", "处暑", "白露", "秋分", "寒露", "霜降", "立冬", "小雪", "大雪", "冬至"
};
static const char *WEEK[7] = { "日", "一", "二", "三", "四", "五", "六" };
static const char *RANGE[12] = {
    "23:00-00:59", "01:00-02:59", "03:00-04:59", "05:00-06:59", "07:00-08:59", "09:00-10:59",
    "11:00-12:59", "13:00-14:59", "15:00-16:59", "17:00-18:59", "19:00-20:59", "21:00-22:59"
};
static const char *NAYIN[30] = {
    "海中金", "炉中火", "大林木", "路旁土", "剑锋金", "山头火", "涧下水", "城头土", "白蜡金", "杨柳木",
    "泉中水", "屋上土", "霹雳火", "松柏木", "长流水", "沙中金", "山下火", "平地木", "壁上土", "金箔金",
    "覆灯火", "天河水", "大驿土", "钗钏金", "桑柘木", "大溪水", "沙中土", "天上火", "石榴木", "大海水"
};
static const char *SX[12] = { "鼠", "牛", "虎", "兔", "龙", "蛇", "马", "羊", "猴", "鸡", "狗", "猪" };

static const uint32_t lunarInfo[201] = {
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
    0x0d520
};

#define SPR 206264.80624709636
static double rad(double d) { return d * 3.14159265358979323846 / 180.0; }
static double norm360(double d) { d = fmod(d, 360.0); return d < 0 ? d + 360 : d; }

static double eLon(double t, int n) {
    t /= 10;
    double v = 0, tn = 1;
    double m0 = GZ_XL0[2] - GZ_XL0[1];
    for (int i = 0; i < 6; i++) {
        int n1 = (int)GZ_XL0[1 + i], n2 = (int)GZ_XL0[2 + i], n0 = n2 - n1;
        if (n0 == 0) continue;
        int m;
        if (n < 0) m = n2;
        else {
            m = (int)floor(3.0 * n * n0 / m0 + 0.5) + n1;
            if (i != 0) m += 3;
            if (m > n2) m = n2;
        }
        double c = 0;
        for (int j = n1; j < m; j += 3) c += GZ_XL0[j] * cos(GZ_XL0[j + 1] + t * GZ_XL0[j + 2]);
        v += c * tn; tn *= t;
    }
    v /= GZ_XL0[0];
    double t2 = t * t, t3 = t2 * t;
    v += (-0.0728 - 2.7702 * t - 1.1019 * t2 - 0.0996 * t3) / SPR;
    return v;
}
static double nutationLon2(double t) {
    double t2 = t * t, dl = 0;
    for (int i = 0; i < (int)(sizeof(GZ_NUT) / sizeof(double)); i += 5) {
        double a = i == 0 ? -1.742 * t : 0;
        dl += (GZ_NUT[i + 3] + a) * sin(GZ_NUT[i] + GZ_NUT[i + 1] * t + GZ_NUT[i + 2] * t2);
    }
    return dl / 100 / SPR;
}
static double gxcSunLon(double t) {
    double v = -0.043126 + 628.301955 * t - 0.000002732 * t * t;
    double e = 0.016708634 - 0.000042037 * t - 0.0000001267 * t * t;
    return -20.49552 * (1 + e * cos(v)) / SPR;
}
static double saLon(double t, int n) { return eLon(t, n) + nutationLon2(t) + gxcSunLon(t) + 3.14159265358979323846; }
static double ev(double t) {
    double f = 628.307585 * t;
    return 628.332 + 21 * sin(1.527 + f) + 0.44 * sin(1.48 + f * 2)
         + 0.129 * sin(5.82 + f) * t + 0.00055 * sin(4.21 + f) * t * t;
}
static double dtExt(double y, double jsd) { double dy = (y - 1820) / 100; return -20 + jsd * dy * dy; }
static double dtCalc(double y) {
    const double *A = GZ_DT;
    int size = (int)(sizeof(GZ_DT) / sizeof(double));
    double y0 = A[size - 2], t0 = A[size - 1];
    if (y >= y0) {
        double jsd = 31;
        if (y > y0 + 100) return dtExt(y, jsd);
        double v = dtExt(y, jsd), dv = dtExt(y0, jsd) - t0;
        return v - dv * (y0 + 100 - y) / 100;
    }
    int i = 0;
    for (i = 0; i < size; i += 5) { if (y < A[i + 5]) break; }
    double t1 = (y - A[i]) / (A[i + 5] - A[i]) * 10, t2 = t1 * t1, t3 = t2 * t1;
    return A[i + 1] + A[i + 2] * t1 + A[i + 3] * t2 + A[i + 4] * t3;
}

/* 节气缓存：连续三年槽位，避免每秒重复级数求和 */
static struct { int year; int64_t ms[24]; bool valid; } termCache[3];
static int64_t termMsUncached(int year, int k) {
    double w = rad(norm360(285 + 15 * k));
    double t = ((year - 2000) * 365.2422 + 5.0 + k * 15.2184) / 36525;
    for (int i = 0; i < 32; i++) {
        double diff = w - saLon(t, -1);
        diff = atan2(sin(diff), cos(diff));
        if (fabs(diff) < 1e-9) break;
        t += diff / ev(t);
    }
    double jde = 2451545 + t * 36525;
    /* 向上取整：整数 ms 的 tm<=t 判定与 JS 浮点时刻完全等价 */
    return (int64_t)ceil((jde - 2440587.5) * 86400000.0 - dtCalc(year) * 1000.0);
}
int64_t gz_term_ms(int year, int k) {
    int slot = ((year % 3) + 3) % 3;
    if (!termCache[slot].valid || termCache[slot].year != year) {
        termCache[slot].year = year;
        for (int i = 0; i < 24; i++) termCache[slot].ms[i] = termMsUncached(year, i);
        termCache[slot].valid = true;
    }
    return termCache[slot].ms[k];
}

static int64_t jdn(int y, int m, int d) {
    int64_t a = (14 - m) / 12, y2 = y + 4800 - a, m2 = m + 12 * a - 3;
    return d + (153 * m2 + 2) / 5 + 365 * y2 + y2 / 4 - y2 / 100 + y2 / 400 - 32045;
}
static int gzIdx(int stem, int branch) { return ((6 * stem - 5 * branch) % 60 + 60) % 60; }

static int leapMonth(int y) { return lunarInfo[y - 1900] & 0xf; }
static int leapDays(int y) { return leapMonth(y) ? ((lunarInfo[y - 1900] & 0x10000) ? 30 : 29) : 0; }
static int monthDays(int y, int m) { return (lunarInfo[y - 1900] & (0x10000 >> m)) ? 30 : 29; }
static int lYearDays(int y) {
    int s = 348;
    for (int i = 0x8000; i > 0x8; i >>= 1) s += (lunarInfo[y - 1900] & i) ? 1 : 0;
    return s + leapDays(y);
}
static void solarToLunar(int Y, int M, int D, int *ly, int *lm, int *ld, bool *isLeapOut) {
    *ly = 0; *lm = 0; *ld = 0; *isLeapOut = false;
    if (Y < 1900 || Y > 2100) return;
    int64_t offset = jdn(Y, M, D) - jdn(1900, 1, 31);
    int lyr = 1900; int64_t temp = 0;
    for (; lyr < 2101 && offset > 0; lyr++) { temp = lYearDays(lyr); offset -= temp; }
    if (offset < 0) { offset += temp; lyr--; }
    int leap = leapMonth(lyr); bool isLeap = false; int lmo = 1;
    for (; lmo < 13 && offset > 0; lmo++) {
        if (leap > 0 && lmo == leap + 1 && !isLeap) { --lmo; isLeap = true; temp = leapDays(lyr); }
        else temp = monthDays(lyr, lmo);
        if (isLeap && lmo == leap + 1) isLeap = false;
        offset -= temp;
    }
    if (offset == 0 && leap > 0 && lmo == leap + 1) {
        if (isLeap) isLeap = false; else { isLeap = true; --lmo; }
    }
    if (offset < 0) { offset += temp; --lmo; }
    *ly = lyr; *lm = lmo; *ld = (int)offset + 1; *isLeapOut = isLeap;
}

static void civil_from_days(int64_t z, int *y, int *m, int *d) {
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t yy = (int64_t)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yy + (*m <= 2));
}

void gz_compute(int64_t t, gz_now_t *o) {
    memset(o, 0, sizeof(*o));
    o->epoch = t;
    int64_t bj = t + 8 * 3600000LL;
    int64_t days = bj >= 0 ? bj / 86400000LL : (bj - 86399999LL) / 86400000LL;
    int64_t rem = bj - days * 86400000LL;
    civil_from_days(days, &o->Y, &o->M, &o->D);
    o->h = (int)(rem / 3600000LL);
    o->mi = (int)(rem / 60000LL % 60);
    o->s = (int)(rem / 1000LL % 60);
    o->dow = (int)(((days + 4) % 7 + 7) % 7);

    int Y = o->Y;
    int64_t lichun = gz_term_ms(Y, 2);
    int gzYear = t < lichun ? Y - 1 : Y;
    int yIdx = ((gzYear - 4) % 60 + 60) % 60;
    int yStem = yIdx % 10;

    static const int jieK[12] = { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22 };
    static const int jieOrder[12] = { 11, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int64_t best = INT64_MIN; int mOrder = 10, bestK = 22;
    for (int i = 0; i < 12; i++) {
        int64_t tm = gz_term_ms(Y, jieK[i]);
        if (tm <= t && tm > best) { best = tm; mOrder = jieOrder[i]; bestK = jieK[i]; }
    }
    if (best == INT64_MIN) { best = gz_term_ms(Y - 1, 22); mOrder = 10; bestK = 22; }
    int mStem = ((yStem % 5) * 2 + 2 + mOrder) % 10;
    int mBranch = (mOrder + 2) % 12;

    int dIdx = (int)((jdn(Y, o->M, o->D) + 49) % 60);
    int dStem = dIdx % 10;
    int hBranch = ((o->h + 1) % 24) / 2;
    int hourDayStem = o->h == 23 ? (dStem + 1) % 10 : dStem;
    int hStem = ((hourDayStem % 5) * 2 + hBranch) % 10;

    o->yIdx = yIdx;
    o->mIdx = gzIdx(mStem, mBranch);
    o->dIdx = dIdx;
    o->hIdx = gzIdx(hStem, hBranch);
    o->hBranch = hBranch;
    o->monthJieK = bestK;

    int64_t ms[27]; int ks[27]; int n = 0;
    ms[n] = gz_term_ms(Y - 1, 23); ks[n++] = 23;
    for (int k = 0; k < 24; k++) { ms[n] = gz_term_ms(Y, k); ks[n++] = k; }
    ms[n] = gz_term_ms(Y + 1, 0); ks[n++] = 0;
    ms[n] = gz_term_ms(Y + 1, 1); ks[n++] = 1;
    int cur = 0, next = 1;
    for (int j = 0; j < n; j++) {
        if (ms[j] <= t) cur = j;
        else { next = j; break; }
    }
    o->termCurK = ks[cur]; o->termCurMs = ms[cur];
    o->termNextK = ks[next]; o->termNextMs = ms[next];
    o->jieqiDay = (int)((t - ms[cur]) / 86400000LL) + 1;

    int lyY, lyM, lyD; bool lyLeap;
    solarToLunar(Y, o->M, o->D, &lyY, &lyM, &lyD, &lyLeap);
    o->lunarM = lyM; o->lunarD = lyD; o->lunarLeap = lyLeap;
    int xs = ((dIdx % 12) - (dIdx % 10) + 12) % 12;
    o->kong[0] = (xs + 10) % 12;
    o->kong[1] = (xs + 11) % 12;
}

void gz_pillar(int idx, char out[7]) {
    memcpy(out, GAN[idx % 10], 3);
    memcpy(out + 3, ZHI[idx % 12], 3);
    out[6] = 0;
}
const char *gz_gan(int i) { return GAN[i % 10]; }
const char *gz_zhi(int i) { return ZHI[i % 12]; }
const char *gz_yue(int m) { return (m >= 1 && m <= 12) ? YUE[m - 1] : ""; }
const char *gz_ri(int d) { return (d >= 1 && d <= 30) ? RI[d - 1] : ""; }
const char *gz_term(int k) { return TERMS[k % 24]; }
const char *gz_week(int dow) { return WEEK[dow % 7]; }
const char *gz_shichen_range(int hb) { return RANGE[hb % 12]; }
const char *gz_nayin(int idx) { return NAYIN[(idx / 2) % 30]; }
const char *gz_shengxiao(int yIdx) { return SX[yIdx % 12]; }

/* ---------------- 六爻 ---------------- */
static const char *REL[5] = { "兄弟", "子孙", "父母", "妻财", "官鬼" };
static const char *SPIRITS[6] = { "青龙", "朱雀", "勾陈", "螣蛇", "白虎", "玄武" };
static const int SPIRIT_START[10] = { 0, 0, 1, 1, 2, 3, 4, 4, 5, 5 };
static const char *STAGE[8] = { "本位卦", "一世卦", "二世卦", "三世卦", "四世卦", "五世卦", "游魂卦", "归魂卦" };
static const char *TRI_NAME[8] = { "乾", "兑", "离", "震", "巽", "坎", "艮", "坤" };
static const int TRI_BITS[8][3] = { {1,1,1},{1,1,0},{1,0,1},{1,0,0},{0,1,1},{0,1,0},{0,0,1},{0,0,0} };
static const int TRI_ELEM[8] = { 3, 3, 1, 0, 0, 4, 2, 2 };
static const int NAJIA[8][6] = {
    {0,2,4,6,8,10}, {5,3,1,11,9,7}, {3,1,11,9,7,5}, {0,2,4,6,8,10},
    {1,11,9,7,5,3}, {2,4,6,8,10,0}, {4,6,8,10,0,2}, {7,5,3,1,11,9}
};
static const int BRANCH_ELEM[12] = { 4, 2, 0, 0, 2, 1, 1, 2, 3, 3, 2, 4 };
static const char *NAMES[8][8] = {
    { "乾为天", "天泽履", "天火同人", "天雷无妄", "天风姤", "天水讼", "天山遁", "天地否" },
    { "泽天夬", "兑为泽", "泽火革", "泽雷随", "泽风大过", "泽水困", "泽山咸", "泽地萃" },
    { "火天大有", "火泽睽", "离为火", "火雷噬嗑", "火风鼎", "火水未济", "火山旅", "火地晋" },
    { "雷天大壮", "雷泽归妹", "雷火丰", "震为雷", "雷风恒", "雷水解", "雷山小过", "雷地豫" },
    { "风天小畜", "风泽中孚", "风火家人", "风雷益", "巽为风", "风水涣", "风山渐", "风地观" },
    { "水天需", "水泽节", "水火既济", "水雷屯", "水风井", "坎为水", "水山蹇", "水地比" },
    { "山天大畜", "山泽损", "山火贲", "山雷颐", "山风蛊", "山水蒙", "艮为山", "山地剥" },
    { "地天泰", "地泽临", "地火明夷", "地雷复", "地风升", "地水师", "地山谦", "坤为地" }
};
static int triByBits[8];
static int palaceOf[64], stageOf[64], shiOf[64];
static bool lyBuilt;

static int hexIdx(const int bits[6]) {
    int lo = triByBits[bits[0] | bits[1] << 1 | bits[2] << 2];
    int up = triByBits[bits[3] | bits[4] << 1 | bits[5] << 2];
    return up * 8 + lo;
}
static void lyBuild(void) {
    if (lyBuilt) return;
    for (int i = 0; i < 8; i++)
        triByBits[TRI_BITS[i][0] | TRI_BITS[i][1] << 1 | TRI_BITS[i][2] << 2] = i;
    static const int shiList[8] = { 5, 0, 1, 2, 3, 4, 3, 2 };
    /* 每世相对上一卦需翻的爻：[ [],[0],[1],[2],[3],[4],[3],[0,1,2] ] */
    static const int flipN[8] = { 0, 1, 1, 1, 1, 1, 1, 3 };
    static const int flipIdx[8][3] = { {0,0,0},{0,0,0},{1,0,0},{2,0,0},{3,0,0},{4,0,0},{3,0,0},{0,1,2} };
    for (int p = 0; p < 8; p++) {
        int cur[6];
        for (int i = 0; i < 3; i++) { cur[i] = TRI_BITS[p][i]; cur[i + 3] = TRI_BITS[p][i]; }
        for (int s = 0; s < 8; s++) {
            for (int f = 0; f < flipN[s]; f++) cur[flipIdx[s][f]] ^= 1;
            int idx = hexIdx(cur);
            palaceOf[idx] = p; stageOf[idx] = s; shiOf[idx] = shiList[s];
        }
    }
    lyBuilt = true;
}
static int relation(int pe, int le) {
    if (le == pe) return 0;
    if ((pe + 1) % 5 == le) return 1;
    if ((le + 1) % 5 == pe) return 2;
    if ((pe + 2) % 5 == le) return 3;
    return 4;
}
void ly_cast(ly_line_t out[6]) {
    for (int i = 0; i < 6; i++) {
        int sum = 0;
        for (int c = 0; c < 3; c++) sum += (esp_random() & 1) ? 3 : 2;
        out[i].value = sum;
        out[i].yang = sum == 7 || sum == 9;
        out[i].moving = sum == 6 || sum == 9;
    }
}
void ly_chart(const ly_line_t lines[6], int dayGan, int dayIdx, ly_chart_t *o) {
    lyBuild();
    memset(o, 0, sizeof(*o));
    for (int i = 0; i < 6; i++) {
        o->lines[i] = lines[i];
        o->bits[i] = lines[i].yang ? 1 : 0;
        o->changedBits[i] = lines[i].moving ? (o->bits[i] ^ 1) : o->bits[i];
        if (lines[i].moving) o->hasMoving = true;
    }
    int idx = hexIdx(o->bits), cIdx = hexIdx(o->changedBits);
    strcpy(o->name, NAMES[idx / 8][idx % 8]);
    strcpy(o->changedName, NAMES[cIdx / 8][cIdx % 8]);
    o->palace = palaceOf[idx]; o->stage = stageOf[idx];
    o->shi = shiOf[idx]; o->ying = (shiOf[idx] + 3) % 6;
    int pe = TRI_ELEM[o->palace];
    int xs = ((dayIdx % 12) - (dayIdx % 10) + 12) % 12;
    o->kong[0] = (xs + 10) % 12; o->kong[1] = (xs + 11) % 12;
    int spStart = SPIRIT_START[dayGan % 10];
    int lo = triByBits[o->bits[0] | o->bits[1] << 1 | o->bits[2] << 2];
    int up = triByBits[o->bits[3] | o->bits[4] << 1 | o->bits[5] << 2];
    int cLo = triByBits[o->changedBits[0] | o->changedBits[1] << 1 | o->changedBits[2] << 2];
    int cUp = triByBits[o->changedBits[3] | o->changedBits[4] << 1 | o->changedBits[5] << 2];
    for (int i = 0; i < 6; i++) {
        int tri = i < 3 ? lo : up, cTri = i < 3 ? cLo : cUp;
        o->zhiIdx[i] = NAJIA[tri][i];
        o->changedZhiIdx[i] = NAJIA[cTri][i];
        o->rel[i] = relation(pe, BRANCH_ELEM[o->zhiIdx[i]]);
        o->spirit[i] = (spStart + i) % 6;
    }
}
const char *ly_rel(int r) { return REL[r % 5]; }
const char *ly_spirit(int s) { return SPIRITS[s % 6]; }
const char *ly_palace_name(int p) { return TRI_NAME[p % 8]; }
const char *ly_stage_name(int s) { return STAGE[s % 8]; }