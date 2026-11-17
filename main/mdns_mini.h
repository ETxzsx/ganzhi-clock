#pragma once
#include "lwip/ip4_addr.h"
void mdns_mini_start(const char *host, const ip4_addr_t *ip);
void mdns_mini_stop(void);