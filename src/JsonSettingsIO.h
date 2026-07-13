#pragma once

#include <vector>

class TinyRdrSettings;
class TinyRdrState;
class WifiCredentialStore;
class RecentBooksStore;
class OpdsServerStore;
struct BookmarkEntry;

namespace JsonSettingsIO {

// TinyRdrSettings
bool saveSettings(const TinyRdrSettings& s, const char* path);
bool loadSettings(TinyRdrSettings& s, const char* json, bool* needsResave = nullptr);

// TinyRdrState
bool saveState(const TinyRdrState& s, const char* path);
bool loadState(TinyRdrState& s, const char* json);

// Bookmarks
bool saveBookmarks(const std::vector<BookmarkEntry>& bookmarks, const char* path);
bool loadBookmarks(std::vector<BookmarkEntry>& bookmarks, const char* json);

}  // namespace JsonSettingsIO
