#include "NerdWaveLog.h"
#include "Browser.h"
#include "Config.h"
extern uint8_t nerdMusicSource();

bool MusicBrowser::begin() { path_ = "/"; selected_ = 0; depth_ = 0; clearCache(); return retry(); }

bool MusicBrowser::retry() {
  clearCache();
  page_.count = 0;
  error_ = "";
  if (!loadPage(selected_)) { error_ = api_.error(); return false; }
  if (selected_ >= page_.total) selected_ = page_.total ? page_.total - 1 : 0;
  return true;
}

void MusicBrowser::clearCache() {
  for (auto &entry : pageCache_) {
    entry.valid = false;
    entry.path = "";
    entry.source = 255;
    entry.stamp = 0;
  }
  cacheClock_ = 0;
}

bool MusicBrowser::loadPage(int index) {
  if (index < 0) return false;
  const uint16_t off = (index / Config::PAGE_SIZE) * Config::PAGE_SIZE;
  if (page_.count && off == page_.offset) return true;
  const uint8_t source = nerdMusicSource();
  for (auto &entry : pageCache_) {
    if (entry.valid && entry.source == source && entry.path == path_ && entry.data.offset == off) {
      MusicPage saved = entry.data;
      if (page_.count && page_.offset != off) {
        // Swap active and cached pages, preserving both sides of the boundary.
        entry.data = page_;
        entry.source = source;
        entry.path = path_;
        entry.stamp = ++cacheClock_;
      } else {
        entry.valid = false;
      }
      page_ = saved;
      error_ = "";
      return true;
    }
  }
  // Only cache a successful fetch; on failure retain the old visible page.
  MusicPage incoming;
  if (!api_.fetch(path_, off, incoming)) { error_ = api_.error(); return false; }
  // Keep the previous page as well so rapid back-and-forth does not reconnect.
  if (page_.count && page_.offset != off) {
    CachedPage* slot = nullptr;
    for (auto &entry : pageCache_) {
      if (entry.valid && entry.source == source && entry.path == path_ && entry.data.offset == page_.offset) {
        slot = &entry; break;
      }
    }
    if (!slot) {
      slot = &pageCache_[0];
      for (auto &entry : pageCache_) {
        if (!entry.valid) { slot = &entry; break; }
        if (entry.stamp < slot->stamp) slot = &entry;
      }
    }
    slot->data = page_;
    slot->path = path_;
    slot->source = source;
    slot->stamp = ++cacheClock_;
    slot->valid = true;
  }
  page_ = incoming;
  error_ = "";
  return true;
}

const MusicItem* MusicBrowser::visibleItem(int index) {
  if (index < 0 || index >= page_.total) return nullptr;
  if (!loadPage(index)) return nullptr;
  int local = index - page_.offset;
  if (local < 0 || local >= page_.count) return nullptr;
  return &page_.items[local];
}

const MusicItem* MusicBrowser::current() { return visibleItem(selected_); }

bool MusicBrowser::move(int delta) {
  if (page_.total == 0 || delta == 0) return false;
  // Umlaufende Navigation, auch bei mehreren Encoderschritten.
  const int total = (int)page_.total;
  int next = ((selected_ + delta) % total + total) % total;
  if (next == selected_) return false;
  int previous = selected_;
  selected_ = next;
  if (!loadPage(selected_)) { selected_ = previous; return false; }
  return true;
}

bool MusicBrowser::openFolder(const String& folder) {
  if (depth_ >= MAX_DEPTH) { error_ = "Maximale Ordnertiefe"; return false; }
  String oldPath = path_;
  int oldSelected = selected_;
  MusicPage oldPage = page_;
  path_ = folder;
  selected_ = 0;
  page_.count = 0;
  if (!retry()) {
    path_ = oldPath; selected_ = oldSelected; page_ = oldPage;
    return false;
  }
  stack_[depth_++] = {oldPath, oldSelected};
  NerdWaveLog::output.printf("Ordner: %s\n", path_.c_str());
  return true;
}

bool MusicBrowser::activate() {
  const MusicItem* item = current();
  if (!item) return false;
  String target = item->path;
  if (item->directory) return openFolder(target);
  selectedTrack_ = target;
  NerdWaveLog::output.printf("Titel gewaehlt: %s\n", selectedTrack_.c_str());
  return true;
}

bool MusicBrowser::back() {
  if (!depth_) return false;
  FolderState prev = stack_[depth_ - 1];
  String oldPath = path_;
  int oldSelected = selected_;
  MusicPage oldPage = page_;
  path_ = prev.path;
  selected_ = prev.selected;
  page_.count = 0;
  if (!retry()) { path_ = oldPath; selected_ = oldSelected; page_ = oldPage; return false; }
  --depth_;
  NerdWaveLog::output.printf("Zurueck: %s, Position %d\n", path_.c_str(), selected_);
  return true;
}

// Reconstruct the navigation stack so Back also works after autostart.
// Uses the same paginated folder API as the normal browser.
bool MusicBrowser::focusTrack(const String& trackPath) {
  if (!trackPath.startsWith("/") || trackPath.endsWith("/")) return false;
  if (!begin()) return false;
  int from = 1;
  while (true) {
    const int slash = trackPath.indexOf('/', from);
    if (slash < 0) break;
    const String folderPath = trackPath.substring(0, slash);
    bool found = false;
    for (int i = 0; i < total(); ++i) {
      const MusicItem* it = visibleItem(i);
      if (!it) return false;
      if (it->directory && it->path == folderPath) {
        selected_ = i;
        found = true;
        break;
      }
    }
    if (!found || !openFolder(folderPath)) return false;
    from = slash + 1;
  }
  for (int i = 0; i < total(); ++i) {
    const MusicItem* it = visibleItem(i);
    if (!it) return false;
    if (!it->directory && it->path == trackPath) {
      selected_ = i;
      selectedTrack_ = trackPath;
      NerdWaveLog::output.printf("[BROWSER] Wiedergabekontext: %s (Index %d)\n", path_.c_str(), selected_);
      return true;
    }
  }
  return false;
}
