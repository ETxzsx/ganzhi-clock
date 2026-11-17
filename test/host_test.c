/* 主机对拍：stdin 每行一个 epoch(ms)；argv[1]=="ly" 时每行 dayGan dayIdx v0..v5 */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include "gz_core.h"

int main(int argc, char **argv) {
    _setmode(_fileno(stdout), _O_BINARY);
    if (argc > 1 && strcmp(argv[1], "term") == 0) {
        int y, k;
        while (scanf("%d %d", &y, &k) == 2) printf("%lld\n", (long long)gz_term_ms(y, k));
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "ly") == 0) {
        int dg, di, v[6];
        while (scanf("%d %d %d %d %d %d %d %d", &dg, &di, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 8) {
            ly_line_t L[6];
            for (int i = 0; i < 6; i++) {
                L[i].value = v[i];
                L[i].yang = v[i] == 7 || v[i] == 9;
                L[i].moving = v[i] == 6 || v[i] == 9;
            }
            ly_chart_t c;
            ly_chart(L, dg, di, &c);
            printf("%s %s %d %d %s%s", c.name, c.changedName, c.shi, c.ying,
                   gz_zhi(c.kong[0]), gz_zhi(c.kong[1]));
            for (int i = 0; i < 6; i++)
                printf(" %s/%s/%s/%d", gz_zhi(c.zhiIdx[i]), ly_rel(c.rel[i]), ly_spirit(c.spirit[i]),
                       c.lines[i].moving ? 1 : 0);
            printf(" %d %d\n", c.palace, c.stage);
        }
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "star") == 0) {
        long long t; double lon;
        while (scanf("%lld %lf", &t, &lon) == 2) {
            gz_star_t s;
            gz_star(t, lon, &s);
            printf("%.3f %d %d %d %d\n", s.lst, s.midIdx, s.sunCi, s.yueJiang, s.hourZhi);
        }
        return 0;
    }
    long long t;
    while (scanf("%lld", &t) == 1) {
        gz_now_t o;
        gz_compute(t, &o);
        char y[7], m[7], d[7], h[7];
        gz_pillar(o.yIdx, y); gz_pillar(o.mIdx, m); gz_pillar(o.dIdx, d); gz_pillar(o.hIdx, h);
        printf("%s %s %s %s|%d %d %d %d|%s%s %s|%s%s|%d %d %d|%s %d\n",
               y, m, d, h, o.Y, o.M, o.D, o.dow,
               o.lunarLeap ? "闰" : "", gz_yue(o.lunarM), gz_ri(o.lunarD),
               gz_zhi(o.kong[0]), gz_zhi(o.kong[1]),
               o.monthJieK, o.termCurK, o.jieqiDay, gz_term(o.termNextK), o.hBranch);
    }
    return 0;
}