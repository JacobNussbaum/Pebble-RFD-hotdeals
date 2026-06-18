#pragma once
#include <pebble.h>

// ---------------------------------------------------------------------------
// Color palette
// ---------------------------------------------------------------------------
#define RFD_RED   GColorRed
#define RFD_WHITE GColorWhite

// ---------------------------------------------------------------------------
// CMD values (watch -> JS)
// ---------------------------------------------------------------------------
#define CMD_FETCH_FEED      1
#define CMD_FETCH_POSTS     2
#define CMD_FETCH_FILTERED  3
#define CMD_SAVE_FILTERS    4

// CMD values (JS -> watch, sent in CMD key)
#define CMD_REPLY_FEED      100
#define CMD_REPLY_POSTS     101

// ---------------------------------------------------------------------------
// Data sizes
// ---------------------------------------------------------------------------
#define MAX_TOPICS_CACHED  40
#define MAX_TOPICS_BATCH   10
#define MAX_POSTS_BATCH     5
#define MAX_TITLE_LEN      61
#define MAX_AUTHOR_LEN     21
#define MAX_BODY_LEN      121

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------
typedef struct {
  int  topic_id;
  char title[MAX_TITLE_LEN];
  int  replies;
  int  score;
} RfdTopic;

typedef struct {
  char author[MAX_AUTHOR_LEN];
  char body[MAX_BODY_LEN];
} RfdPost;

// ---------------------------------------------------------------------------
// Global state (owned by comm.c)
// ---------------------------------------------------------------------------
extern RfdTopic g_topics[MAX_TOPICS_CACHED];
extern int      g_topic_count;
extern int      g_total_topics;

extern RfdPost  g_posts[MAX_POSTS_BATCH];
extern int      g_post_count;
extern int      g_post_total_pages;
extern int      g_post_current_page;
extern int      g_current_topic_id;

extern bool     g_is_loading;
extern bool     g_is_filtered;