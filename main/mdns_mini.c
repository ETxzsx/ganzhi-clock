#include "mdns_mini.h"
#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/igmp.h"
#include "esp_log.h"

static const char *TAG = "mdns";
static volatile bool s_run;
static TaskHandle_t s_task;
static char s_name[64];
static ip4_addr_t s_ip;

static int encode_name(uint8_t *p, const char *host)
{
    int n = 0;
    const char *s = host;
    while (*s) {
        const char *dot = strchr(s, '.');
        int seg = dot ? (int)(dot - s) : (int)strlen(s);
        p[n++] = (uint8_t)seg;
        memcpy(p + n, s, seg);
        n += seg;
        s = dot ? dot + 1 : s + seg;
    }
    p[n++] = 0;
    return n;
}
static int build_answer(uint8_t *out, uint16_t id, const uint8_t *question, int qlen, bool with_question)
{
    int o = 0;
    out[o++] = id >> 8; out[o++] = id & 0xFF;
    out[o++] = 0x84; out[o++] = 0x00;
    out[o++] = 0; out[o++] = with_question ? 1 : 0;
    out[o++] = 0; out[o++] = 1;
    out[o++] = 0; out[o++] = 0; out[o++] = 0; out[o++] = 0;
    int name_off = o;
    if (with_question) {
        memcpy(out + o, question, qlen);
        o += qlen;
    } else {
        o += encode_name(out + o, s_name);
    }
    out[o++] = 0; out[o++] = 1;
    out[o++] = 0x80; out[o++] = 1;
    out[o++] = 0; out[o++] = 0; out[o++] = 0; out[o++] = 120;
    out[o++] = 0; out[o++] = 4;
    memcpy(out + o, &s_ip.addr, 4); o += 4;
    (void)name_off;
    return o;
}
static void send_mcast(int sock, uint8_t *buf, int len, struct sockaddr_in *dst)
{
    sendto(sock, buf, len, 0, (struct sockaddr *)dst, sizeof(*dst));
}
static void mdns_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) { s_task = NULL; vTaskDelete(NULL); return; }
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);
    struct sockaddr_in bind_addr = { 0 };
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(5353);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&bind_addr, sizeof bind_addr) < 0) {
        close(sock); s_task = NULL; vTaskDelete(NULL); return;
    }
    ip4_addr_t group;
    IP4_ADDR(&group, 224, 0, 0, 251);
    igmp_joingroup(IP4_ADDR_ANY4, &group);
    struct sockaddr_in mcast = { 0 };
    mcast.sin_family = AF_INET;
    mcast.sin_port = htons(5353);
    mcast.sin_addr.s_addr = group.addr;
    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    uint8_t out[512];
    int n = build_answer(out, 0, NULL, 0, false);
    send_mcast(sock, out, n, &mcast);
    vTaskDelay(pdMS_TO_TICKS(1000));
    send_mcast(sock, out, n, &mcast);
    ESP_LOGI(TAG, "announced %s", s_name);

    uint8_t buf[512];
    char qname[128];
    while (s_run) {
        struct sockaddr_in src;
        socklen_t slen = sizeof src;
        int len = recvfrom(sock, buf, sizeof buf, 0, (struct sockaddr *)&src, &slen);
        if (len < 12) continue;
        if (buf[2] & 0x80) continue;
        uint16_t qd = (buf[4] << 8) | buf[5];
        if (qd != 1) continue;
        int pos = 12, qi = 0;
        while (pos < len && buf[pos] && qi < (int)sizeof qname - 1) {
            int seg = buf[pos];
            if (pos + 1 + seg > len) break;
            if (qi) qname[qi++] = '.';
            for (int i = 0; i < seg && qi < (int)sizeof qname - 1; i++)
                qname[qi++] = (char)tolower(buf[pos + 1 + i]);
            pos += seg + 1;
        }
        qname[qi] = 0;
        if (pos + 5 > len) continue;
        uint16_t qtype = (buf[pos + 1] << 8) | buf[pos + 2];
        int qlen = pos + 5 - 12;
        if ((qtype == 1 || qtype == 255) && strcmp(qname, s_name) == 0) {
            n = build_answer(out, 0, buf + 12, qlen, true);
            send_mcast(sock, out, n, &mcast);
        }
    }
    igmp_leavegroup(IP4_ADDR_ANY4, &group);
    close(sock);
    ESP_LOGI(TAG, "stopped");
    s_task = NULL;
    vTaskDelete(NULL);
}

void mdns_mini_start(const char *host, const ip4_addr_t *ip)
{
    if (s_task) return;
    snprintf(s_name, sizeof s_name, "%s.local", host);
    s_ip = *ip;
    s_run = true;
    xTaskCreate(mdns_task, "mdns_mini", 4096, NULL, 4, &s_task);
}
void mdns_mini_stop(void)
{
    s_run = false;
    for (int i = 0; i < 25 && s_task; i++) vTaskDelay(pdMS_TO_TICKS(50));
}