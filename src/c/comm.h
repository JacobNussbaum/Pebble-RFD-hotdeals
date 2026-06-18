#include <pebble.h>
#pragma once
#include <pebble.h>

void comm_init(void);
void comm_deinit(void);
void comm_fetch_feed(int page, int batch, bool filtered);
void comm_fetch_posts(int topic_id, int page);
