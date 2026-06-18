// RFD Hot Deals - PebbleKit JS (runs on phone, bridges internet to watch)
// =========================================================================
// Communication flow:
//   Watch C app  --[AppMessage CMD]--> PKJS --> RFD API --> PKJS --> Watch C app
//
// CMDs sent from watch:
//   1 = fetch feed page N  (includes PAGE_NUM)
//   2 = fetch posts for topic (includes TOPIC_ID, PAGE_NUM)
//   3 = fetch filtered feed   (includes PAGE_NUM)
//   4 = save filter keywords  (includes FILTER_KEYWORDS csv string)

var RFD_BASE   = 'https://forums.redflagdeals.com/api/';
var FORUM_ID   = 9;      // Hot Deals forum
var PER_PAGE   = 20;     // Keep batches small for AppMessage limits
var BATCH_SIZE = 10;     // Topics sent per AppMessage round-trip

// ---- Utility ---------------------------------------------------------------

function stripHtml(str) {
  if (!str) return '';
  return str
    .replace(/<br\s*\/?>/gi, ' ')
    .replace(/<[^>]+>/g, '')
    .replace(/&amp;/g, '&')
    .replace(/&lt;/g, '<')
    .replace(/&gt;/g, '>')
    .replace(/&quot;/g, '"')
    .replace(/&#039;/g, "'")
    .replace(/&nbsp;/g, ' ')
    .replace(/\s+/g, ' ')
    .trim();
}

function truncate(str, maxLen) {
  if (!str) return '';
  str = String(str);
  if (str.length <= maxLen) return str;
  return str.substring(0, maxLen - 1) + '…';
}

function xhrGet(url, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function() {
    if (this.status >= 200 && this.status < 300) {
      try {
        var json = JSON.parse(this.responseText);
        callback(null, json);
      } catch (e) {
        callback('JSON parse error: ' + e.message, null);
      }
    } else {
      callback('HTTP ' + this.status, null);
    }
  };
  xhr.onerror = function() {
    callback('Network error', null);
  };
  xhr.open('GET', url);
  xhr.setRequestHeader('User-Agent', 'RFDPebbleApp/1.0');
  xhr.send();
}

function sendError(msg) {
  Pebble.sendAppMessage({ 'ERROR_MSG': truncate(msg, 64) }, null, null);
}

function sendStatus(msg) {
  Pebble.sendAppMessage({ 'STATUS_MSG': truncate(msg, 32) }, null, null);
}

// ---- Filter helpers --------------------------------------------------------

function getFilters() {
  var raw = localStorage.getItem('filterKeywords') || '';
  if (!raw) return [];
  return raw.split(',')
    .map(function(k) { return k.trim().toLowerCase(); })
    .filter(function(k) { return k.length > 0; });
}

function matchesFilters(title, filters) {
  if (!filters || filters.length === 0) return false;
  var t = title.toLowerCase();
  for (var i = 0; i < filters.length; i++) {
    if (t.indexOf(filters[i]) !== -1) return true;
  }
  return false;
}

// ---- Feed fetching ---------------------------------------------------------

// Fetches a page of topics and sends up to BATCH_SIZE topics to the watch.
// batchIndex: which batch within the page (0 = first BATCH_SIZE topics)
// filtered: if true, apply keyword filter
function fetchFeed(page, batchIndex, filtered) {
  page = page || 1;
  batchIndex = batchIndex || 0;

  var url = RFD_BASE + 'topics?forum_id=' + FORUM_ID +
    '&per_page=' + PER_PAGE + '&page=' + page;

  sendStatus('Loading...');

  xhrGet(url, function(err, data) {
    if (err) {
      sendError('Fetch failed: ' + err);
      return;
    }

    var topics = (data && data.topics) ? data.topics : [];
    var total  = (data && data.pager) ? (data.pager.total_results || topics.length) : topics.length;

    // Apply filter if requested
    if (filtered) {
      var filters = getFilters();
      topics = topics.filter(function(t) {
        return matchesFilters(t.title || '', filters);
      });
    }

    // Slice to batch
    var start = batchIndex * BATCH_SIZE;
    var batch = topics.slice(start, start + BATCH_SIZE);

    if (batch.length === 0) {
      sendError(filtered ? 'No matches' : 'No deals found');
      return;
    }

    var msg = {
      'CMD': 100,               // reply: feed batch
      'TOPIC_COUNT': batch.length,
      'BATCH_INDEX': batchIndex,
      'TOTAL_TOPICS': Math.min(topics.length, 40)
    };

    for (var i = 0; i < batch.length && i < BATCH_SIZE; i++) {
      var t = batch[i];
      var title = truncate(stripHtml(t.title || 'Untitled'), 60);
      var tid   = t.topic_id || t.id || 0;
      var replies = t.num_replies || t.reply_count || 0;
      var score   = t.score || t.votes || 0;

      msg['TOPIC_TITLE_'   + i] = title;
      msg['TOPIC_ID_'      + i] = tid;
      msg['TOPIC_REPLIES_' + i] = replies;
      msg['TOPIC_SCORE_'   + i] = score;
    }

    Pebble.sendAppMessage(msg,
      function() { console.log('Feed batch sent, count=' + batch.length); },
      function(e) { console.log('Feed send failed: ' + JSON.stringify(e)); }
    );
  });
}

// ---- Posts fetching --------------------------------------------------------

// Fetches one page of posts for a topic and sends up to 5 posts to the watch.
function fetchPosts(topicId, page) {
  page = page || 1;

  // RFD API endpoint for thread posts
  var url = RFD_BASE + 'topics/' + topicId + '/posts?per_page=5&page=' + page;

  sendStatus('Loading...');

  xhrGet(url, function(err, data) {
    if (err) {
      sendError('Posts failed: ' + err);
      return;
    }

    var posts = (data && data.posts) ? data.posts : [];
    var total = (data && data.pager) ? (data.pager.total_pages || 1) : 1;

    if (posts.length === 0) {
      sendError('No posts found');
      return;
    }

    var msg = {
      'CMD': 101,             // reply: posts batch
      'POST_COUNT': posts.length,
      'PAGE_NUM': page,
      'TOTAL_TOPICS': total   // reuse field for total pages
    };

    for (var i = 0; i < posts.length && i < 5; i++) {
      var p = posts[i];
      var author = truncate(stripHtml(p.username || p.author || 'anon'), 20);
      var body   = truncate(stripHtml(p.comment || p.body || ''), 120);
      msg['POST_AUTHOR_' + i] = author;
      msg['POST_BODY_'   + i] = body;
    }

    Pebble.sendAppMessage(msg,
      function() { console.log('Posts sent page=' + page); },
      function(e) { console.log('Posts send failed: ' + JSON.stringify(e)); }
    );
  });
}

// ---- Configuration (Android settings page) ---------------------------------

Pebble.addEventListener('showConfiguration', function() {
  var filters = localStorage.getItem('filterKeywords') || '';
  var encodedFilters = encodeURIComponent(filters);
  var configUrl =
    'https://cdn.rawgit.com/bonrkus/rfd-pebble-config/main/config.html' +
    '?filters=' + encodedFilters;

  // FALLBACK: If you haven't hosted a config page yet, use an inline data: URI
  // that prompts for comma-separated keywords.  Replace configUrl with this:
  var fallbackHtml = [
    '<!DOCTYPE html><html><head>',
    '<meta name="viewport" content="width=device-width,initial-scale=1">',
    '<style>',
    'body{font-family:sans-serif;background:#c0392b;color:#fff;padding:16px;margin:0}',
    'h2{margin:0 0 12px}',
    'textarea{width:100%;height:120px;border:none;border-radius:6px;padding:8px;',
    'font-size:14px;box-sizing:border-box}',
    'p{font-size:12px;opacity:0.85}',
    'button{margin-top:12px;width:100%;padding:12px;background:#fff;color:#c0392b;',
    'border:none;border-radius:6px;font-size:16px;font-weight:700;cursor:pointer}',
    '</style></head><body>',
    '<h2>RFD Filter Keywords</h2>',
    '<p>Enter keywords (comma-separated). The Filtered Feed (hold ✓) will only show',
    ' deals whose title contains at least one keyword.</p>',
    '<textarea id="kw" placeholder="e.g. Costco, GPU, Amazon, Switch"></textarea>',
    '<button onclick="save()">Save &amp; Close</button>',
    '<script>',
    'var d=decodeURIComponent(location.search.replace(/.*[?&]filters=/,"").replace(/&.*/,""))||"";',
    'document.getElementById("kw").value=d;',
    'function save(){',
    '  var v=document.getElementById("kw").value.trim();',
    '  location.href="pebblejs://close#"+encodeURIComponent(JSON.stringify({filters:v}));',
    '}',
    '</script></body></html>'
  ].join('');

  // Use data URI for zero-dependency config page
  var dataUri = 'data:text/html,' + encodeURIComponent(fallbackHtml)
    .replace(/filters%3D/, 'filters=' + encodedFilters);

  Pebble.openURL(dataUri);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e.response || e.response === 'CANCELLED') return;
  try {
    var cfg = JSON.parse(decodeURIComponent(e.response));
    if (cfg.filters !== undefined) {
      localStorage.setItem('filterKeywords', cfg.filters);
      console.log('Saved filters: ' + cfg.filters);
      // Notify watch that filters updated
      Pebble.sendAppMessage({ 'STATUS_MSG': 'Filters saved!' }, null, null);
    }
  } catch(ex) {
    console.log('Config parse error: ' + ex);
  }
});

// ---- Main message handler --------------------------------------------------

Pebble.addEventListener('ready', function() {
  console.log('RFD PebbleKit JS ready');
});

Pebble.addEventListener('appmessage', function(e) {
  var dict = e.payload;
  var cmd  = dict['CMD'];

  if (cmd === 1) {
    // Fetch regular hot deals feed
    var page  = dict['PAGE_NUM']    || 1;
    var batch = dict['BATCH_INDEX'] || 0;
    fetchFeed(page, batch, false);

  } else if (cmd === 2) {
    // Fetch posts for a specific topic
    var tid  = dict['TOPIC_ID']  || 0;
    var page = dict['PAGE_NUM']  || 1;
    fetchPosts(tid, page);

  } else if (cmd === 3) {
    // Fetch filtered feed
    var page  = dict['PAGE_NUM']    || 1;
    var batch = dict['BATCH_INDEX'] || 0;
    fetchFeed(page, batch, true);

  } else if (cmd === 4) {
    // Save filter keywords from watch (alternative to config page)
    var kw = dict['FILTER_KEYWORDS'] || '';
    localStorage.setItem('filterKeywords', kw);
    sendStatus('Filters saved!');

  } else {
    console.log('Unknown CMD: ' + cmd);
  }
});
