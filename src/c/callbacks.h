#include <pebble.h>
#pragma once

// Called by comm.c when data arrives or errors occur.
// Implemented in callbacks.c, which dispatches to the right window.
void on_feed_updated(void);
void on_posts_updated(void);
void on_status_message(const char *msg);
void on_error_message(const char *msg);