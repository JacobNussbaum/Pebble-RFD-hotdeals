#include <pebble.h>
// callbacks.c
// Central dispatch for comm.c -> window callbacks.
// Checks which window is on top and routes accordingly.

#include <pebble.h>
#include "rfd_app.h"
#include "callbacks.h"
#include "feed_window.h"
#include "posts_window.h"

void on_feed_updated(void) {
  feed_window_on_data_ready();
}

void on_posts_updated(void) {
  posts_window_on_data_ready();
}

void on_status_message(const char *msg) {
  feed_window_on_status(msg);
}

void on_error_message(const char *msg) {
  feed_window_on_error(msg);
  posts_window_on_error(msg);
}