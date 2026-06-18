#include <pebble.h>
// rfd_app.c — RFD Hot Deals Pebble Watchapp
// Main entry point. Manages the window stack and global state.

#include <pebble.h>
#include "rfd_app.h"
#include "feed_window.h"
#include "posts_window.h"
#include "comm.h"

// ---------------------------------------------------------------------------
// App entry
// ---------------------------------------------------------------------------

#include <pebble.h>
#include "rfd_app.h"
#include "callbacks.h"
#include "feed_window.h"
#include "posts_window.h"
#include "comm.h"


static void init(void) {
  comm_init();
  feed_window_push(false);   // false = regular feed
}

static void deinit(void) {
  comm_deinit();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
