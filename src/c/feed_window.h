#pragma once
#include <pebble.h>

void feed_window_push(bool filtered);
void feed_window_reload(void);

// Called from callbacks.c
void feed_window_on_data_ready(void);
void feed_window_on_status(const char *msg);
void feed_window_on_error(const char *msg);