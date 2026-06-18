// feed_window.c
// Main hot-deals feed using a MenuLayer.
// SELECT       = open thread
// LONG SELECT  = toggle filtered feed
// BACK         = exit

#include <pebble.h>
#include "rfd_app.h"
#include "comm.h"
#include "feed_window.h"
#include "posts_window.h"

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static Window    *s_window      = NULL;
static MenuLayer *s_menu_layer  = NULL;
static TextLayer *s_status_layer = NULL;
static Layer     *s_header_layer = NULL;

static bool s_filtered     = false;
static int  s_current_page = 1;
static char s_status_buf[48];

// ---------------------------------------------------------------------------
// Header draw — solid red bar
// ---------------------------------------------------------------------------
static void header_draw(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, RFD_RED);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  graphics_context_set_text_color(ctx, RFD_WHITE);
  graphics_draw_text(ctx,
    s_filtered ? "RFD  FILTERED" : "RFD  HOT DEALS",
    fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
    GRect(0, 2, b.size.w, 18),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// ---------------------------------------------------------------------------
// MenuLayer callbacks
// ---------------------------------------------------------------------------
static uint16_t get_num_sections(MenuLayer *ml, void *ctx) { return 1; }

static uint16_t get_num_rows(MenuLayer *ml, uint16_t section, void *ctx) {
  if (g_topic_count == 0) return 1;
  return (g_topic_count < g_total_topics) ? g_topic_count + 1 : g_topic_count;
}

static int16_t get_cell_height(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  return 44;
}

static int16_t get_header_height(MenuLayer *ml, uint16_t section, void *ctx) {
  return 0;
}

static void draw_row(GContext *ctx, const Layer *cell_layer,
                     MenuIndex *idx, void *context) {
  GRect b = layer_get_bounds(cell_layer);
  int row = idx->row;

  if (g_topic_count == 0) {
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx,
      g_is_loading ? "Loading..." : "SELECT to refresh",
      fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(8, 13, b.size.w - 16, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  // "Load more" row
  if (row >= g_topic_count) {
    graphics_context_set_text_color(ctx, RFD_RED);
    graphics_draw_text(ctx, "Load more...",
      fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
      GRect(8, 13, b.size.w - 16, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }

  RfdTopic *t = &g_topics[row];

  // Title
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, t->title,
    fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
    GRect(8, 2, b.size.w - 16, 28),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Meta line
  char meta[32];
  snprintf(meta, sizeof(meta), "^%d  %d replies", t->score, t->replies);
  graphics_context_set_text_color(ctx, GColorDarkGray);
  graphics_draw_text(ctx, meta,
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    GRect(8, 28, b.size.w - 16, 14),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void select_click(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  int row = idx->row;
  if (g_topic_count == 0) {
    g_topic_count = 0;
    comm_fetch_feed(1, 0, s_filtered);
    return;
  }
  if (row >= g_topic_count) {
    int next_batch = g_topic_count / 10;
    int next_page  = s_current_page;
    if (g_topic_count > 0 && g_topic_count % 20 == 0) {
      next_page++;
      s_current_page = next_page;
      next_batch = 0;
    }
    comm_fetch_feed(next_page, next_batch, s_filtered);
    return;
  }
  g_current_topic_id = g_topics[row].topic_id;
  posts_window_push(g_topics[row].topic_id, g_topics[row].title);
}

static void select_long_click(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  s_filtered     = !s_filtered;
  g_is_filtered  = s_filtered;
  g_topic_count  = 0;
  g_total_topics = 0;
  s_current_page = 1;
  layer_mark_dirty(s_header_layer);
  menu_layer_reload_data(s_menu_layer);
  comm_fetch_feed(1, 0, s_filtered);
}

// ---------------------------------------------------------------------------
// Callbacks from callbacks.c
// ---------------------------------------------------------------------------
void feed_window_on_data_ready(void) {
  if (!s_menu_layer) return;
  menu_layer_reload_data(s_menu_layer);
  if (s_status_layer) text_layer_set_text(s_status_layer, "");
}

void feed_window_on_status(const char *msg) {
  if (!s_status_layer) return;
  strncpy(s_status_buf, msg, sizeof(s_status_buf) - 1);
  s_status_buf[sizeof(s_status_buf) - 1] = '\0';
  text_layer_set_text(s_status_layer, s_status_buf);
}

void feed_window_on_error(const char *msg) {
  if (!s_window) return;
  strncpy(s_status_buf, msg, sizeof(s_status_buf) - 1);
  s_status_buf[sizeof(s_status_buf) - 1] = '\0';
  if (s_status_layer) text_layer_set_text(s_status_layer, s_status_buf);
  if (s_menu_layer)   menu_layer_reload_data(s_menu_layer);
}

// ---------------------------------------------------------------------------
// Window lifecycle
// ---------------------------------------------------------------------------
static void window_load(Window *window) {
  Layer *root   = window_get_root_layer(window);
  GRect  bounds = layer_get_bounds(root);

  // Red header bar (20px)
  s_header_layer = layer_create(GRect(0, 0, bounds.size.w, 20));
  layer_set_update_proc(s_header_layer, header_draw);
  layer_add_child(root, s_header_layer);

  // Status bar (bottom 16px)
  s_status_layer = text_layer_create(
    GRect(0, bounds.size.h - 16, bounds.size.w, 16));
  text_layer_set_background_color(s_status_layer, RFD_RED);
  text_layer_set_text_color(s_status_layer, RFD_WHITE);
  text_layer_set_font(s_status_layer,
    fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  text_layer_set_text(s_status_layer, "");
  layer_add_child(root, text_layer_get_layer(s_status_layer));

  // MenuLayer
  GRect menu_frame = GRect(0, 20, bounds.size.w, bounds.size.h - 36);
  s_menu_layer = menu_layer_create(menu_frame);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_sections  = get_num_sections,
    .get_num_rows      = get_num_rows,
    .get_cell_height   = get_cell_height,
    .get_header_height = get_header_height,
    .draw_row          = draw_row,
    .select_click      = select_click,
    .select_long_click = select_long_click,
  });
  menu_layer_set_highlight_colors(s_menu_layer, RFD_RED, RFD_WHITE);
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_menu_layer));

  // Initial fetch
  text_layer_set_text(s_status_layer, "Loading...");
  comm_fetch_feed(1, 0, s_filtered);
}

static void window_unload(Window *window) {
  if (s_menu_layer)   { menu_layer_destroy(s_menu_layer);    s_menu_layer   = NULL; }
  if (s_status_layer) { text_layer_destroy(s_status_layer);  s_status_layer = NULL; }
  if (s_header_layer) { layer_destroy(s_header_layer);        s_header_layer = NULL; }
  window_destroy(window);
  s_window = NULL;
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------
void feed_window_push(bool filtered) {
  s_filtered     = filtered;
  g_is_filtered  = filtered;
  s_current_page = 1;
  s_window = window_create();
  window_set_background_color(s_window, RFD_WHITE);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

void feed_window_reload(void) {
  g_topic_count  = 0;
  g_total_topics = 0;
  s_current_page = 1;
  if (s_status_layer) text_layer_set_text(s_status_layer, "Refreshing...");
  comm_fetch_feed(1, 0, s_filtered);
}