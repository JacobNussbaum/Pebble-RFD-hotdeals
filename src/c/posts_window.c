#include <pebble.h>
#include "rfd_app.h"
#include "comm.h"
#include "posts_window.h"

static Window      *s_window       = NULL;
static ScrollLayer *s_scroll_layer = NULL;
static TextLayer   *s_author_layer = NULL;
static TextLayer   *s_body_layer   = NULL;
static TextLayer   *s_header_layer = NULL;
static TextLayer   *s_nav_layer    = NULL;
static Layer       *s_header_bg    = NULL;
static Layer       *s_shadow_layer = NULL;

static int  s_topic_id   = 0;
static int  s_post_index = 0;
static char s_title[MAX_TITLE_LEN];
static char s_author_buf[MAX_AUTHOR_LEN + 4];
static char s_nav_buf[28];

// ---------------------------------------------------------------------------
// Shadow
// ---------------------------------------------------------------------------
static void shadow_draw(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  GColor shades[] = { GColorDarkGray, GColorLightGray, GColorLightGray, GColorWhite };
  for (int i = 0; i < 4; i++) {
    graphics_context_set_fill_color(ctx, shades[i]);
    graphics_fill_rect(ctx, GRect(0, i, b.size.w, 1), 0, GCornerNone);
  }
}

// ---------------------------------------------------------------------------
// Display update
// ---------------------------------------------------------------------------
static void update_display(void) {
  if (!s_window || g_post_count == 0) return;
  RfdPost *p = &g_posts[s_post_index];

  snprintf(s_author_buf, sizeof(s_author_buf), "@%s", p->author);
  text_layer_set_text(s_author_layer, s_author_buf);
  text_layer_set_text(s_body_layer, p->body);

  int global_num = (g_post_current_page - 1) * 5 + s_post_index + 1;
  snprintf(s_nav_buf, sizeof(s_nav_buf),
    "Post %d  pg %d/%d", global_num,
    g_post_current_page, g_post_total_pages);
  text_layer_set_text(s_nav_layer, s_nav_buf);

  GRect root_b   = layer_get_bounds(window_get_root_layer(s_window));
  int   content_w = root_b.size.w - 16;
  GSize body_size = text_layer_get_content_size(s_body_layer);
  int   body_h    = body_size.h + 8;
  if (body_h < 60) body_h = 60;

  layer_set_frame(text_layer_get_layer(s_body_layer),
    GRect(8, 32, content_w, body_h));

  int total_h = 32 + body_h + 8;
  if (total_h < root_b.size.h - 44) total_h = root_b.size.h - 44;
  scroll_layer_set_content_size(s_scroll_layer, GSize(root_b.size.w, total_h));
  scroll_layer_set_content_offset(s_scroll_layer, GPointZero, false);
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------
static void go_next(void) {
  if (s_post_index < g_post_count - 1) {
    s_post_index++;
    update_display();
  } else if (g_post_current_page < g_post_total_pages) {
    text_layer_set_text(s_nav_layer, "Loading...");
    comm_fetch_posts(s_topic_id, g_post_current_page + 1);
  }
}

static void go_prev(void) {
  if (s_post_index > 0) {
    s_post_index--;
    update_display();
  } else if (g_post_current_page > 1) {
    text_layer_set_text(s_nav_layer, "Loading...");
    comm_fetch_posts(s_topic_id, g_post_current_page - 1);
  }
}

static void up_click(ClickRecognizerRef ref, void *ctx)   { go_prev(); }
static void down_click(ClickRecognizerRef ref, void *ctx) { go_next(); }

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP,   up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

// ---------------------------------------------------------------------------
// Callbacks from callbacks.c
// ---------------------------------------------------------------------------
void posts_window_on_data_ready(void) {
  if (!s_window) return;
  s_post_index = 0;
  update_display();
}

void posts_window_on_error(const char *msg) {
  if (!s_window || !s_nav_layer) return;
  strncpy(s_nav_buf, msg, sizeof(s_nav_buf) - 1);
  s_nav_buf[sizeof(s_nav_buf) - 1] = '\0';
  text_layer_set_text(s_nav_layer, s_nav_buf);
}

// ---------------------------------------------------------------------------
// Header draw
// ---------------------------------------------------------------------------
static void header_bg_draw(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, RFD_RED);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
}

// ---------------------------------------------------------------------------
// Window lifecycle
// ---------------------------------------------------------------------------
static void window_load(Window *window) {
  Layer *root   = window_get_root_layer(window);
  GRect  bounds = layer_get_bounds(root);

  // Red header bg — 24px
  s_header_bg = layer_create(GRect(0, 0, bounds.size.w, 24));
  layer_set_update_proc(s_header_bg, header_bg_draw);
  layer_add_child(root, s_header_bg);

  // Title in header
  s_header_layer = text_layer_create(GRect(4, 3, bounds.size.w - 8, 18));
  text_layer_set_background_color(s_header_layer, GColorClear);
  text_layer_set_text_color(s_header_layer, RFD_WHITE);
  text_layer_set_font(s_header_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_overflow_mode(s_header_layer,
    GTextOverflowModeTrailingEllipsis);
  text_layer_set_text(s_header_layer, s_title);
  layer_add_child(root, text_layer_get_layer(s_header_layer));

  // Shadow below header
  s_shadow_layer = layer_create(GRect(0, 24, bounds.size.w, 4));
  layer_set_update_proc(s_shadow_layer, shadow_draw);
  layer_add_child(root, s_shadow_layer);

  // Nav bar at bottom — red, 20px
  s_nav_layer = text_layer_create(
    GRect(0, bounds.size.h - 20, bounds.size.w, 20));
  text_layer_set_background_color(s_nav_layer, RFD_RED);
  text_layer_set_text_color(s_nav_layer, RFD_WHITE);
  text_layer_set_font(s_nav_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_nav_layer, GTextAlignmentCenter);
  text_layer_set_text(s_nav_layer, "Loading...");
  layer_add_child(root, text_layer_get_layer(s_nav_layer));

  // Shadow above nav bar
  Layer *bottom_shadow = layer_create(GRect(0, bounds.size.h - 24, bounds.size.w, 4));
  layer_set_update_proc(bottom_shadow, shadow_draw);
  layer_add_child(root, bottom_shadow);

  // ScrollLayer for post content
  GRect scroll_frame = GRect(0, 28, bounds.size.w, bounds.size.h - 52);
  s_scroll_layer = scroll_layer_create(scroll_frame);
  scroll_layer_set_content_size(s_scroll_layer,
    GSize(bounds.size.w, scroll_frame.size.h));
  layer_add_child(root, scroll_layer_get_layer(s_scroll_layer));

  // Author — red bold
  s_author_layer = text_layer_create(GRect(8, 4, bounds.size.w - 16, 20));
  text_layer_set_background_color(s_author_layer, GColorClear);
  text_layer_set_text_color(s_author_layer, RFD_RED);
  text_layer_set_font(s_author_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  scroll_layer_add_child(s_scroll_layer, text_layer_get_layer(s_author_layer));

  // Body text
  s_body_layer = text_layer_create(
    GRect(8, 32, bounds.size.w - 16, scroll_frame.size.h - 32));
  text_layer_set_background_color(s_body_layer, GColorClear);
  text_layer_set_text_color(s_body_layer, GColorBlack);
  text_layer_set_font(s_body_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_overflow_mode(s_body_layer, GTextOverflowModeWordWrap);
  scroll_layer_add_child(s_scroll_layer, text_layer_get_layer(s_body_layer));

  window_set_click_config_provider(window, click_config);
  comm_fetch_posts(s_topic_id, 1);
}

static void window_unload(Window *window) {
  if (s_scroll_layer)  { scroll_layer_destroy(s_scroll_layer);  s_scroll_layer  = NULL; }
  if (s_author_layer)  { text_layer_destroy(s_author_layer);    s_author_layer  = NULL; }
  if (s_body_layer)    { text_layer_destroy(s_body_layer);      s_body_layer    = NULL; }
  if (s_header_layer)  { text_layer_destroy(s_header_layer);    s_header_layer  = NULL; }
  if (s_nav_layer)     { text_layer_destroy(s_nav_layer);       s_nav_layer     = NULL; }
  if (s_header_bg)     { layer_destroy(s_header_bg);            s_header_bg     = NULL; }
  if (s_shadow_layer)  { layer_destroy(s_shadow_layer);         s_shadow_layer  = NULL; }
  window_destroy(window);
  s_window = NULL;
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------
void posts_window_push(int topic_id, const char *topic_title) {
  s_topic_id   = topic_id;
  s_post_index = 0;
  strncpy(s_title, topic_title ? topic_title : "Thread", MAX_TITLE_LEN - 1);
  s_title[MAX_TITLE_LEN - 1] = '\0';

  s_window = window_create();
  window_set_background_color(s_window, RFD_WHITE);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}