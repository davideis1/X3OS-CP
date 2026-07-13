#pragma once
#include <functional>
#include <vector>

#include "./FileBrowserActivity.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

struct RecentBook;
struct Rect;

// Home is a top "dock" row (continue-reading cover + a "Recents" link) followed by a fixed
// 4x2 icon grid. The dock row and the grid are two separate selectable regions stitched together
// by a single selectorIndex: [0, dockCount) is the dock, [dockCount, dockCount + gridItemCount)
// is the grid. dockCount is 1 (Recents link only) when there's no book to continue, or 2 (cover +
// Recents link) once a recent book exists.
class HomeActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;
  bool recentsLoading = false;
  bool recentsLoaded = false;
  bool firstRenderDone = false;
  bool coverRendered = false;      // Track if cover has been rendered once
  bool coverBufferStored = false;  // Track if cover buffer is stored
  uint8_t* coverBuffer = nullptr;  // HomeActivity's own buffer for cover image
  size_t coverBufferSize = 0;      // Bytes allocated to coverBuffer
  // Logical rect last passed to drawRecentBookCover. The cover snapshot only
  // needs to cover this region, not the entire framebuffer, so we cache the
  // tile instead of all 48 KB. Set in render() before the call.
  int coverRectX = 0;
  int coverRectY = 0;
  int coverRectW = 0;
  int coverRectH = 0;
  std::vector<RecentBook> recentBooks;
  const HomeMenuItem initialMenuItem;

  // Fixed grid tile -> destination mapping (Home no longer has a variable item count: OPDS access
  // is folded into the Library tile's chooser instead of a separate top-level slot).
  static HomeMenuItem gridItemAt(int index);
  static int gridIndexFor(HomeMenuItem item);

  int dockCount() const { return recentBooks.empty() ? 1 : 2; }
  bool isInDock() const { return selectorIndex < dockCount(); }
  int gridIndex() const { return selectorIndex - dockCount(); }

  void onSelectBook(const std::string& path);
  void onGridConfirm(HomeMenuItem item);

  bool storeCoverBuffer();    // Store frame buffer for cover image
  bool restoreCoverBuffer();  // Restore frame buffer from stored cover
  void freeCoverBuffer();     // Free the stored cover buffer
  void loadRecentBooks(int maxBooks);
  void loadRecentCovers(int coverHeight);

 public:
  static constexpr int gridColumns = 4;
  static constexpr int gridItemCount = 8;

  explicit HomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                        HomeMenuItem initialMenuItemValue = HomeMenuItem::NONE)
      : Activity("Home", renderer, mappedInput), initialMenuItem(initialMenuItemValue) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
