#pragma once
#include "MusicApi.h"

class MusicBrowser {
public:
  explicit MusicBrowser(MusicApi& api) : api_(api) {}
  bool begin();
  bool move(int delta);
  bool activate();
  bool back();
  bool retry();
  // Restore browser directory and selection from a full track path.
  bool focusTrack(const String& trackPath);
  int index() const { return selected_; }
  int total() const { return page_.total; }
  int pageStart() const { return page_.offset; }
  int pageCount() const { return page_.count; }
  const String& path() const { return path_; }
  const String& selectedTrack() const { return selectedTrack_; }
  const MusicItem* visibleItem(int index);
  const MusicItem* current();
  String error() const { return error_; }
private:
  struct FolderState { String path; int selected; };
  MusicApi& api_;
  MusicPage page_;
  String path_ = "/", selectedTrack_, error_;
  int selected_ = 0;
  static constexpr int MAX_DEPTH = 12;
  FolderState stack_[MAX_DEPTH];
  int depth_ = 0;
  // Previously visited pages stay cached while navigating the same folder.
  static constexpr uint8_t PAGE_CACHE_SIZE = 4;
  struct CachedPage { MusicPage data; String path; uint8_t source = 255; uint32_t stamp = 0; bool valid = false; };
  CachedPage pageCache_[PAGE_CACHE_SIZE];
  uint32_t cacheClock_ = 0;
  void clearCache();
  bool loadPage(int index);
  bool openFolder(const String& folder);
};
