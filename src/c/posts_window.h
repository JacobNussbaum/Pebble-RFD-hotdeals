#pragma once
#include <pebble.h>

void posts_window_push(int topic_id, const char *topic_title);

// Called from callbacks.c
void posts_window_on_data_ready(void);
void posts_window_on_error(const char *msg);