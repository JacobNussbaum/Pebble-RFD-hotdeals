#include <pebble.h>
#include "rfd_app.h"
#include "callbacks.h"
#include "comm.h"

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
RfdTopic g_topics[MAX_TOPICS_CACHED];
int      g_topic_count       = 0;
int      g_total_topics      = 0;

RfdPost  g_posts[MAX_POSTS_BATCH];
int      g_post_count        = 0;
int      g_post_total_pages  = 1;
int      g_post_current_page = 1;
int      g_current_topic_id  = 0;

bool     g_is_loading        = false;
bool     g_is_filtered       = false;

// ---------------------------------------------------------------------------
// Outbox callbacks
// ---------------------------------------------------------------------------
static void outbox_sent_cb(DictionaryIterator *iter, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_INFO, "AppMessage sent OK");
}

static void outbox_failed_cb(DictionaryIterator *iter,
                              AppMessageResult result, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "AppMessage send failed: %d", (int)result);
  g_is_loading = false;
  on_error_message("Send failed");
}

static void inbox_dropped_cb(AppMessageResult result, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "AppMessage dropped: %d", (int)result);
}

// ---------------------------------------------------------------------------
// Inbox handler
// ---------------------------------------------------------------------------
static void inbox_received_cb(DictionaryIterator *iter, void *ctx) {

  Tuple *status_t = dict_find(iter, MESSAGE_KEY_STATUS_MSG);
  if (status_t) {
    on_status_message(status_t->value->cstring);
    return;
  }
  Tuple *error_t = dict_find(iter, MESSAGE_KEY_ERROR_MSG);
  if (error_t) {
    g_is_loading = false;
    on_error_message(error_t->value->cstring);
    return;
  }

  Tuple *cmd_t = dict_find(iter, MESSAGE_KEY_CMD);
  if (!cmd_t) return;
  int cmd = (int)cmd_t->value->int32;

  if (cmd == CMD_REPLY_FEED) {
    Tuple *count_t = dict_find(iter, MESSAGE_KEY_TOPIC_COUNT);
    Tuple *batch_t = dict_find(iter, MESSAGE_KEY_BATCH_INDEX);
    Tuple *total_t = dict_find(iter, MESSAGE_KEY_TOTAL_TOPICS);

    int count = count_t ? (int)count_t->value->int32 : 0;
    int batch = batch_t ? (int)batch_t->value->int32 : 0;
    int total = total_t ? (int)total_t->value->int32 : count;

    g_total_topics = total;
    int base = batch * MAX_TOPICS_BATCH;

    uint32_t title_keys[MAX_TOPICS_BATCH] = {
      MESSAGE_KEY_TOPIC_TITLE_0, MESSAGE_KEY_TOPIC_TITLE_1,
      MESSAGE_KEY_TOPIC_TITLE_2, MESSAGE_KEY_TOPIC_TITLE_3,
      MESSAGE_KEY_TOPIC_TITLE_4, MESSAGE_KEY_TOPIC_TITLE_5,
      MESSAGE_KEY_TOPIC_TITLE_6, MESSAGE_KEY_TOPIC_TITLE_7,
      MESSAGE_KEY_TOPIC_TITLE_8, MESSAGE_KEY_TOPIC_TITLE_9
    };
    uint32_t id_keys[MAX_TOPICS_BATCH] = {
      MESSAGE_KEY_TOPIC_ID_0, MESSAGE_KEY_TOPIC_ID_1,
      MESSAGE_KEY_TOPIC_ID_2, MESSAGE_KEY_TOPIC_ID_3,
      MESSAGE_KEY_TOPIC_ID_4, MESSAGE_KEY_TOPIC_ID_5,
      MESSAGE_KEY_TOPIC_ID_6, MESSAGE_KEY_TOPIC_ID_7,
      MESSAGE_KEY_TOPIC_ID_8, MESSAGE_KEY_TOPIC_ID_9
    };
    uint32_t reply_keys[MAX_TOPICS_BATCH] = {
      MESSAGE_KEY_TOPIC_REPLIES_0, MESSAGE_KEY_TOPIC_REPLIES_1,
      MESSAGE_KEY_TOPIC_REPLIES_2, MESSAGE_KEY_TOPIC_REPLIES_3,
      MESSAGE_KEY_TOPIC_REPLIES_4, MESSAGE_KEY_TOPIC_REPLIES_5,
      MESSAGE_KEY_TOPIC_REPLIES_6, MESSAGE_KEY_TOPIC_REPLIES_7,
      MESSAGE_KEY_TOPIC_REPLIES_8, MESSAGE_KEY_TOPIC_REPLIES_9
    };
    uint32_t score_keys[MAX_TOPICS_BATCH] = {
      MESSAGE_KEY_TOPIC_SCORE_0, MESSAGE_KEY_TOPIC_SCORE_1,
      MESSAGE_KEY_TOPIC_SCORE_2, MESSAGE_KEY_TOPIC_SCORE_3,
      MESSAGE_KEY_TOPIC_SCORE_4, MESSAGE_KEY_TOPIC_SCORE_5,
      MESSAGE_KEY_TOPIC_SCORE_6, MESSAGE_KEY_TOPIC_SCORE_7,
      MESSAGE_KEY_TOPIC_SCORE_8, MESSAGE_KEY_TOPIC_SCORE_9
    };

    for (int i = 0; i < count && i < MAX_TOPICS_BATCH; i++) {
      int idx = base + i;
      if (idx >= MAX_TOPICS_CACHED) break;

      Tuple *tt = dict_find(iter, title_keys[i]);
      Tuple *ti = dict_find(iter, id_keys[i]);
      Tuple *tr = dict_find(iter, reply_keys[i]);
      Tuple *ts = dict_find(iter, score_keys[i]);

      if (tt) {
        strncpy(g_topics[idx].title, tt->value->cstring, MAX_TITLE_LEN - 1);
        g_topics[idx].title[MAX_TITLE_LEN - 1] = '\0';
      }
      g_topics[idx].topic_id = ti ? (int)ti->value->int32 : 0;
      g_topics[idx].replies  = tr ? (int)tr->value->int32 : 0;
      g_topics[idx].score    = ts ? (int)ts->value->int32 : 0;

      if (idx + 1 > g_topic_count) g_topic_count = idx + 1;
    }

    g_is_loading = false;
    on_feed_updated();

  } else if (cmd == CMD_REPLY_POSTS) {
    Tuple *count_t = dict_find(iter, MESSAGE_KEY_POST_COUNT);
    Tuple *page_t  = dict_find(iter, MESSAGE_KEY_PAGE_NUM);
    Tuple *total_t = dict_find(iter, MESSAGE_KEY_TOTAL_TOPICS);

    g_post_count        = count_t ? (int)count_t->value->int32 : 0;
    g_post_current_page = page_t  ? (int)page_t->value->int32  : 1;
    g_post_total_pages  = total_t ? (int)total_t->value->int32 : 1;

    uint32_t author_keys[MAX_POSTS_BATCH] = {
      MESSAGE_KEY_POST_AUTHOR_0, MESSAGE_KEY_POST_AUTHOR_1,
      MESSAGE_KEY_POST_AUTHOR_2, MESSAGE_KEY_POST_AUTHOR_3,
      MESSAGE_KEY_POST_AUTHOR_4
    };
    uint32_t body_keys[MAX_POSTS_BATCH] = {
      MESSAGE_KEY_POST_BODY_0, MESSAGE_KEY_POST_BODY_1,
      MESSAGE_KEY_POST_BODY_2, MESSAGE_KEY_POST_BODY_3,
      MESSAGE_KEY_POST_BODY_4
    };

    for (int i = 0; i < g_post_count && i < MAX_POSTS_BATCH; i++) {
      Tuple *ta = dict_find(iter, author_keys[i]);
      Tuple *tb = dict_find(iter, body_keys[i]);
      if (ta) {
        strncpy(g_posts[i].author, ta->value->cstring, MAX_AUTHOR_LEN - 1);
        g_posts[i].author[MAX_AUTHOR_LEN - 1] = '\0';
      }
      if (tb) {
        strncpy(g_posts[i].body, tb->value->cstring, MAX_BODY_LEN - 1);
        g_posts[i].body[MAX_BODY_LEN - 1] = '\0';
      }
    }

    g_is_loading = false;
    on_posts_updated();
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void comm_init(void) {
  app_message_register_inbox_received(inbox_received_cb);
  app_message_register_inbox_dropped(inbox_dropped_cb);
  app_message_register_outbox_sent(outbox_sent_cb);
  app_message_register_outbox_failed(outbox_failed_cb);
  app_message_open(app_message_inbox_size_maximum(),
                   app_message_outbox_size_maximum());
}

void comm_deinit(void) {
  app_message_deregister_callbacks();
}

void comm_fetch_feed(int page, int batch, bool filtered) {
  if (g_is_loading) return;
  g_is_loading = true;
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    g_is_loading = false;
    return;
  }
  dict_write_int32(iter, MESSAGE_KEY_CMD,
                   filtered ? CMD_FETCH_FILTERED : CMD_FETCH_FEED);
  dict_write_int32(iter, MESSAGE_KEY_PAGE_NUM,    page);
  dict_write_int32(iter, MESSAGE_KEY_BATCH_INDEX, batch);
  app_message_outbox_send();
}

void comm_fetch_posts(int topic_id, int page) {
  if (g_is_loading) return;
  g_is_loading = true;
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    g_is_loading = false;
    return;
  }
  dict_write_int32(iter, MESSAGE_KEY_CMD,      CMD_FETCH_POSTS);
  dict_write_int32(iter, MESSAGE_KEY_TOPIC_ID, topic_id);
  dict_write_int32(iter, MESSAGE_KEY_PAGE_NUM, page);
  app_message_outbox_send();
}