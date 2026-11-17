#pragma once

typedef struct {
    char *buf;
    int cap;
    int len;
    int need_comma;
    int err;
} mj_t;

void mj_init(mj_t *m, char *buf, int cap);
void mj_obj_begin(mj_t *m);
void mj_obj_end(mj_t *m);
void mj_arr_begin(mj_t *m);
void mj_arr_end(mj_t *m);
void mj_key(mj_t *m, const char *key);
void mj_str(mj_t *m, const char *val);
void mj_int(mj_t *m, long long val);
void mj_bool(mj_t *m, int val);
void mj_kv_str(mj_t *m, const char *key, const char *val);
void mj_kv_int(mj_t *m, const char *key, long long val);
void mj_kv_bool(mj_t *m, const char *key, int val);
const char *mj_done(mj_t *m);

int mj_get_str(const char *json, const char *key, char *out, int outsz);
int mj_get_int(const char *json, const char *key, int *out);
int mj_get_ll(const char *json, const char *key, long long *out);