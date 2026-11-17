#include "dns_portal.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "esp_log.h"

static const char *TAG = "dns_portal";
static volatile bool s_run;
static TaskHandle_t s_task;

static void dns_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) { ESP_LOGE(TAG, "socket fail"); s_task = NULL; vTaskDelete(NULL); return; }
    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&addr, sizeof addr) < 0) {
        ESP_LOGE(TAG, "bind fail"); close(sock); s_task = NULL; vTaskDelete(NULL); return;
    }
    struct timeval tv = { .tv_sec = 0, .tv_usec = 400000 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    uint8_t buf[512], out[512];
    ESP_LOGI(TAG, "captive DNS up");
    while (s_run) {
        struct sockaddr_in src;
        socklen_t slen = sizeof src;
        int len = recvfrom(sock, buf, sizeof buf, 0, (struct sockaddr *)&src, &slen);
        if (len < 12) continue;
        uint16_t qd = (buf[4] << 8) | buf[5];
        if (qd != 1) continue;
        int pos = 12;
        while (pos < len && buf[pos]) pos += buf[pos] + 1;
        if (pos + 5 > len) continue;
        int qend = pos + 5;
        uint16_t qtype = (buf[pos + 1] << 8) | buf[pos + 2];
        if (qtype != 1) continue;
        memcpy(out, buf, qend);
        out[2] = 0x85; out[3] = 0x80;
        out[6] = 0; out[7] = 1;
        out[8] = 0; out[9] = 0; out[10] = 0; out[11] = 0;
        int o = qend;
        out[o++] = 0xC0; out[o++] = 0x0C;
        out[o++] = 0; out[o++] = 1;
        out[o++] = 0; out[o++] = 1;
        out[o++] = 0; out[o++] = 0; out[o++] = 0; out[o++] = 60;
        out[o++] = 0; out[o++] = 4;
        out[o++] = 192; out[o++] = 168; out[o++] = 4; out[o++] = 1;
        sendto(sock, out, o, 0, (struct sockaddr *)&src, slen);
    }
    close(sock);
    ESP_LOGI(TAG, "captive DNS down");
    s_task = NULL;
    vTaskDelete(NULL);
}

void dns_portal_start(void)
{
    if (s_task) return;
    s_run = true;
    xTaskCreate(dns_task, "dns_portal", 4096, NULL, 5, &s_task);
}
void dns_portal_stop(void)
{
    s_run = false;
    for (int i = 0; i < 20 && s_task; i++) vTaskDelay(pdMS_TO_TICKS(50));
}