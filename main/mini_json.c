#include "mini_json.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static void put(mj_t *m, const char *s, int n)
{
    if (m->err || m->len + n >= m->cap) { m->err = 1; return; }
    memcpy(m->buf + m->len, s, n);
    m->len += n;
    m->buf[m->len] = 0;
}
static void puts_(mj_t *m, const char *s) { put(m, s, (int)strlen(s)); }
static void comma(mj_t *m)
{
    if (m->need_comma) puts_(m, ",");
    m->need_comma = 0;
}
void mj_init(mj_t *m, char *buf, int cap)
{
    m->buf = buf; m->cap = cap; m->len = 0; m->need_comma = 0; m->err = 0;
    buf[0] = 0;
}
void mj_obj_begin(mj_t *m) { comma(m); puts_(m, "{"); m->need_comma = 0; }
void mj_obj_end(mj_t *m) { puts_(m, "}"); m->need_comma = 1; }
void mj_arr_begin(mj_t *m) { comma(m); puts_(m, "["); m->need_comma = 0; }
void mj_arr_end(mj_t *m) { puts_(m, "]"); m->need_comma = 1; }
static void esc(mj_t *m, const char *s)
{
    puts_(m, "\"");
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') { char b[2] = { '\\', (char)c }; put(m, b, 2); }
        else if (c == '\n') puts_(m, "\\n");
        else if (c == '\r') puts_(m, "\\r");
        else if (c == '\t') puts_(m, "\\t");
        else if (c < 0x20) { char b[7]; snprintf(b, sizeof b, "\\u%04x", c); puts_(m, b); }
        else { char b[1] = { (char)c }; put(m, b, 1); }
    }
    puts_(m, "\"");
}
void mj_key(mj_t *m, const char *key) { comma(m); esc(m, key); puts_(m, ":"); m->need_comma = 0; }
void mj_str(mj_t *m, const char *val) { comma(m); esc(m, val ? val : ""); m->need_comma = 1; }
void mj_int(mj_t *m, long long val) { comma(m); char b[24]; snprintf(b, sizeof b, "%lld", val); puts_(m, b); m->need_comma = 1; }
void mj_bool(mj_t *m, int val) { comma(m); puts_(m, val ? "true" : "false"); m->need_comma = 1; }
void mj_kv_str(mj_t *m, const char *key, const char *val) { mj_key(m, key); mj_str(m, val); }
void mj_kv_int(mj_t *m, const char *key, long long val) { mj_key(m, key); mj_int(m, val); }
void mj_kv_bool(mj_t *m, const char *key, int val) { mj_key(m, key); mj_bool(m, val); }
const char *mj_done(mj_t *m) { return m->err ? "{}" : m->buf; }

static const char *find_key(const char *json, const char *key)
{
    char pat[80];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *p = json;
    while ((p = strstr(p, pat)) != NULL) {
        const char *q = p + strlen(pat);
        while (*q && isspace((unsigned char)*q)) q++;
        if (*q == ':') return q + 1;
        p = q;
    }
    return NULL;
}
static int utf8_encode(unsigned cp, char *out)
{
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) { out[0] = (char)(0xC0 | (cp >> 6)); out[1] = (char)(0x80 | (cp & 63)); return 2; }
    out[0] = (char)(0xE0 | (cp >> 12)); out[1] = (char)(0x80 | ((cp >> 6) & 63)); out[2] = (char)(0x80 | (cp & 63));
    return 3;
}
int mj_get_str(const char *json, const char *key, char *out, int outsz)
{
    const char *p = find_key(json, key);
    if (!p) return -1;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '"') return -1;
    p++;
    int n = 0;
    while (*p && *p != '"') {
        char c = *p++;
        if (c == '\\') {
            char e = *p++;
            switch (e) {
            case 'n': c = '\n'; break;
            case 'r': c = '\r'; break;
            case 't': c = '\t'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'u': {
                unsigned cp = 0;
                for (int i = 0; i < 4 && isxdigit((unsigned char)*p); i++) {
                    char h = *p++;
                    cp = cp * 16 + (h <= '9' ? h - '0' : (tolower(h) - 'a' + 10));
                }
                if (cp >= 0xD800 && cp <= 0xDBFF && p[0] == '\\' && p[1] == 'u') {
                    unsigned lo = 0; p += 2;
                    for (int i = 0; i < 4 && isxdigit((unsigned char)*p); i++) {
                        char h = *p++;
                        lo = lo * 16 + (h <= '9' ? h - '0' : (tolower(h) - 'a' + 10));
                    }
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                char u[4]; int un = utf8_encode(cp, u);
                for (int i = 0; i < un && n < outsz - 1; i++) out[n++] = u[i];
                continue;
            }
            default: c = e; break;
            }
        }
        if (n < outsz - 1) out[n++] = c;
    }
    out[n] = 0;
    return n;
}
int mj_get_int(const char *json, const char *key, int *out)
{
    const char *p = find_key(json, key);
    if (!p) return -1;
    while (*p && isspace((unsigned char)*p)) p++;
    char *end;
    long v = strtol(p, &end, 10);
    if (end == p) return -1;
    *out = (int)v;
    return 0;
}
int mj_get_ll(const char *json, const char *key, long long *out)
{
    const char *p = find_key(json, key);
    if (!p) return -1;
    while (*p && isspace((unsigned char)*p)) p++;
    char *end;
    long long v = strtoll(p, &end, 10);
    if (end == p) return -1;
    *out = v;
    return 0;
}