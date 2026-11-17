#pragma once
#include <stdint.h>
#include <stdbool.h>

/* 干支历（与 www/core.js 同算法：寿星 VSOP87 截断级数，节气误差约1秒） */
typedef struct {
    int64_t epoch;
    int Y, M, D, h, mi, s, dow;
    int yIdx, mIdx, dIdx, hIdx;   /* 六十甲子序 0..59 */
    int hBranch;                  /* 时辰支 0..11 */
    int kong[2];                  /* 旬空支序 */
    int lunarM, lunarD;           /* 农历月/日 1..12 / 1..30 */
    bool lunarLeap;
    int monthJieK;                /* 月柱所依节气 */
    int termCurK, termNextK;
    int64_t termCurMs, termNextMs;
    int jieqiDay;
} gz_now_t;

void gz_compute(int64_t epoch_ms, gz_now_t *o);
int64_t gz_term_ms(int year, int k);
void gz_pillar(int idx, char out[7]);
const char *gz_gan(int i);
const char *gz_zhi(int i);
const char *gz_yue(int m);
const char *gz_ri(int d);
const char *gz_term(int k);
const char *gz_week(int dow);
const char *gz_shichen_range(int hb);
const char *gz_nayin(int idx);
const char *gz_shengxiao(int yIdx);

/* 六爻（与 www/liuyao.js 同表同算法） */
typedef struct { bool yang, moving; int value; } ly_line_t;
typedef struct {
    ly_line_t lines[6];
    char name[16], changedName[16];
    int bits[6], changedBits[6];
    int shi, ying;
    int kong[2];
    int zhiIdx[6], rel[6], spirit[6];
    int changedZhiIdx[6];
    bool hasMoving;
    int palace, stage;
} ly_chart_t;

void ly_cast(ly_line_t out[6]);
void ly_chart(const ly_line_t lines[6], int dayGan, int dayIdx, ly_chart_t *o);
const char *ly_rel(int r);
const char *ly_spirit(int s);
const char *ly_palace_name(int p);
const char *ly_stage_name(int s);