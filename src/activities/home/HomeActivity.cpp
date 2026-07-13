#include "HomeActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Utf8.h>
#include <Xtc.h>

#include <cstring>
#include <vector>

#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "TinyRdrSettings.h"
#include "TinyRdrState.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/GridNavigator.h"

namespace {
struct GridEntry {
  HomeMenuItem item;
  UIIcon icon;
  StrId label;
};
// Fixed grid order (index == position in the 4x2 grid, row-major). OPDS access lives inside the
// Library tile's chooser (LibraryChoiceActivity) instead of a separate top-level slot.
constexpr GridEntry kGrid[HomeActivity::gridItemCount] = {
    {HomeMenuItem::FILE_BROWSER, UIIcon::Library, StrId::STR_LIBRARY},
    {HomeMenuItem::TOOLS, UIIcon::Tools, StrId::STR_TOOLS},
    {HomeMenuItem::GAMES, UIIcon::Games, StrId::STR_GAMES},
    {HomeMenuItem::FILE_TRANSFER, UIIcon::Wifi, StrId::STR_CONNECT},
    {HomeMenuItem::TODO, UIIcon::Todo, StrId::STR_TODO},
    {HomeMenuItem::NOTES, UIIcon::Notes, StrId::STR_NOTES},
    {HomeMenuItem::WEATHER, UIIcon::Weather, StrId::STR_WEATHER},
    {HomeMenuItem::SETTINGS_MENU, UIIcon::Settings, StrId::STR_SETTINGS_TITLE},
};
}  // namespace

HomeMenuItem HomeActivity::gridItemAt(int index) { return kGrid[index].item; }

int HomeActivity::gridIndexFor(HomeMenuItem item) {
  for (int i = 0; i < gridItemCount; ++i) {
    if (kGrid[i].item == item) return i;
  }
  return -1;
}

void HomeActivity::loadRecentBooks(int maxBooks) {
  recentBooks.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  recentBooks.reserve(std::min(static_cast<int>(books.size()), maxBooks));

  for (const RecentBook& book : books) {
    // Limit to maximum number of recent books
    if (recentBooks.size() >= maxBooks) {
      break;
    }

    // Skip if file no longer exists
    if (RecentBooksStore::isMissing(book)) {
      continue;
    }

    recentBooks.push_back(book);
  }
}

void HomeActivity::loadRecentCovers(int coverHeight) {
  recentsLoading = true;
  bool showingLoading = false;
  Rect popupRect;

  int progress = 0;
  for (RecentBook& book : recentBooks) {
    if (!book.coverBmpPath.empty()) {
      std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight);
      if (!Storage.exists(coverPath.c_str())) {
        // If epub, try to load the metadata for title/author and cover
        if (FsHelpers::hasEpubExtension(book.path)) {
          Epub epub(book.path, "/.tinyrdr");
          // Skip loading css since we only need metadata here
          epub.load(false, true);

          // Try to generate thumbnail image for Continue Reading card
          if (!showingLoading) {
            showingLoading = true;
            popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
          }
          GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
          bool success = epub.generateThumbBmp(coverHeight);
          if (!success) {
            RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
            book.coverBmpPath = "";
          }
          coverRendered = false;
          requestUpdate();
        } else if (FsHelpers::hasXtcExtension(book.path)) {
          // Handle XTC file
          Xtc xtc(book.path, "/.tinyrdr");
          if (xtc.load()) {
            // Try to generate thumbnail image for Continue Reading card
            if (!showingLoading) {
              showingLoading = true;
              popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
            }
            GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
            bool success = xtc.generateThumbBmp(coverHeight);
            if (!success) {
              RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
              book.coverBmpPath = "";
            }
            coverRendered = false;
            requestUpdate();
          }
        }
      }
    }
    progress++;
  }

  recentsLoaded = true;
  recentsLoading = false;
}

void HomeActivity::onEnter() {
  Activity::onEnter();

  const auto& metrics = UITheme::getInstance().getMetrics();
  loadRecentBooks(metrics.homeRecentBooksCount);

  if (initialMenuItem == HomeMenuItem::RECENTS) {
    selectorIndex = dockCount() - 1;
  } else if (initialMenuItem == HomeMenuItem::NONE) {
    selectorIndex = 0;
  } else {
    const int gi = gridIndexFor(initialMenuItem);
    selectorIndex = gi >= 0 ? dockCount() + gi : 0;
  }

  // Trigger first update
  requestUpdate();
}

void HomeActivity::onExit() {
  Activity::onExit();

  // Free the stored cover buffer if any
  freeCoverBuffer();
}

bool HomeActivity::storeCoverBuffer() {
  // render() must have already set the cover rect; without it we'd be back to
  // cloning the whole framebuffer.
  if (coverRectW <= 0 || coverRectH <= 0) return false;
  freeCoverBuffer();
  const size_t needed = renderer.getRegionByteSize(coverRectX, coverRectY, coverRectW, coverRectH);
  if (needed == 0) return false;
  coverBuffer = static_cast<uint8_t*>(malloc(needed));
  if (!coverBuffer) {
    LOG_ERR("HOME", "OOM: cover buffer (%u bytes)", (unsigned)needed);
    return false;
  }
  coverBufferSize = needed;
  if (!renderer.copyRegionToBuffer(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer, coverBufferSize)) {
    free(coverBuffer);
    coverBuffer = nullptr;
    coverBufferSize = 0;
    return false;
  }
  return true;
}

bool HomeActivity::restoreCoverBuffer() {
  if (!coverBuffer || coverRectW <= 0 || coverRectH <= 0) return false;
  return renderer.copyBufferToRegion(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer, coverBufferSize);
}

void HomeActivity::freeCoverBuffer() {
  if (coverBuffer) {
    free(coverBuffer);
    coverBuffer = nullptr;
  }
  coverBufferSize = 0;
  coverBufferStored = false;
}

void HomeActivity::loop() {
  using Button = MappedInputManager::Button;

  buttonNavigator.onPressAndContinuous({Button::Left}, [this] {
    if (isInDock()) {
      selectorIndex = (selectorIndex - 1 + dockCount()) % dockCount();
    } else {
      int gi = gridIndex();
      GridNavigator::moveLeft(gi, gridColumns, gridItemCount);
      selectorIndex = dockCount() + gi;
    }
    requestUpdate();
  });

  buttonNavigator.onPressAndContinuous({Button::Right}, [this] {
    if (isInDock()) {
      selectorIndex = (selectorIndex + 1) % dockCount();
    } else {
      int gi = gridIndex();
      GridNavigator::moveRight(gi, gridColumns, gridItemCount);
      selectorIndex = dockCount() + gi;
    }
    requestUpdate();
  });

  buttonNavigator.onPressAndContinuous({Button::Up}, [this] {
    if (!isInDock()) {
      const int gi = gridIndex();
      if (gi < gridColumns) {
        selectorIndex = 0;  // Top row of the grid backs out to the dock row
      } else {
        int newGi = gi;
        GridNavigator::moveUp(newGi, gridColumns, gridItemCount);
        selectorIndex = dockCount() + newGi;
      }
      requestUpdate();
    }
  });

  buttonNavigator.onPressAndContinuous({Button::Down}, [this] {
    if (isInDock()) {
      selectorIndex = dockCount();  // Enter the grid at column 0, row 0
    } else {
      int gi = gridIndex();
      GridNavigator::moveDown(gi, gridColumns, gridItemCount);
      selectorIndex = dockCount() + gi;
    }
    requestUpdate();
  });

  if (mappedInput.wasReleased(Button::Confirm)) {
    if (isInDock()) {
      if (dockCount() == 2 && selectorIndex == 0) {
        onSelectBook(recentBooks[0].path);
      } else {
        activityManager.goToRecentBooks();
      }
    } else {
      onGridConfirm(gridItemAt(gridIndex()));
    }
  }
}

void HomeActivity::onGridConfirm(HomeMenuItem item) {
  switch (item) {
    case HomeMenuItem::FILE_BROWSER:
      activityManager.goToLibrary();
      break;
    case HomeMenuItem::TOOLS:
      activityManager.goToTools();
      break;
    case HomeMenuItem::GAMES:
      activityManager.goToComingSoon(StrId::STR_GAMES);
      break;
    case HomeMenuItem::FILE_TRANSFER:
      activityManager.goToFileTransfer();
      break;
    case HomeMenuItem::TODO:
      activityManager.goToComingSoon(StrId::STR_TODO);
      break;
    case HomeMenuItem::NOTES:
      activityManager.goToComingSoon(StrId::STR_NOTES);
      break;
    case HomeMenuItem::WEATHER:
      activityManager.goToComingSoon(StrId::STR_WEATHER);
      break;
    case HomeMenuItem::SETTINGS_MENU:
      activityManager.goToSettings();
      break;
    default:
      break;
  }
}

void HomeActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  bool bufferRestored = coverBufferStored && restoreCoverBuffer();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.homeTopPadding}, nullptr);

  // Record the tile rect so storeCoverBuffer (called from the theme) knows
  // which sub-region of the framebuffer to snapshot. ~16 KB in Portrait
  // instead of the 48 KB full framebuffer the previous bind captured.
  coverRectX = 0;
  coverRectY = metrics.homeTopPadding;
  coverRectW = pageWidth;
  coverRectH = metrics.homeCoverTileHeight;

  GUI.drawRecentBookCover(renderer, Rect{0, metrics.homeTopPadding, pageWidth, metrics.homeCoverTileHeight},
                          recentBooks, selectorIndex, coverRendered, coverBufferStored, bufferRestored,
                          std::bind(&HomeActivity::storeCoverBuffer, this));

  // "Recents" link: always the last dock slot (index dockCount() - 1).
  const bool recentsSelected = isInDock() && selectorIndex == dockCount() - 1;
  const int recentsY = metrics.homeTopPadding + metrics.homeCoverTileHeight + metrics.verticalSpacing;
  const int sidePad = metrics.contentSidePadding;
  const int recentsWidth = pageWidth - sidePad * 2;
  if (recentsSelected) {
    renderer.fillRect(sidePad, recentsY, recentsWidth, metrics.menuRowHeight);
  } else {
    renderer.drawRect(sidePad, recentsY, recentsWidth, metrics.menuRowHeight);
  }
  const int recentsLineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  renderer.drawCenteredText(UI_10_FONT_ID, recentsY + (metrics.menuRowHeight - recentsLineHeight) / 2,
                            tr(STR_RECENTS_LINK), !recentsSelected);

  std::vector<GridTile> tiles;
  tiles.reserve(gridItemCount);
  for (const auto& entry : kGrid) {
    tiles.push_back(GridTile{entry.icon, I18N.get(entry.label)});
  }

  const int gridY = recentsY + metrics.menuRowHeight + metrics.homeMenuTopOffset;
  const int gridHeight = pageHeight - gridY - metrics.buttonHintsHeight - metrics.verticalSpacing;
  GUI.drawIconGrid(renderer, Rect{sidePad, gridY, pageWidth - sidePad * 2, gridHeight}, gridColumns, tiles,
                   isInDock() ? -1 : gridIndex());

  const auto labels = mappedInput.mapLabels("", tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();

  if (!firstRenderDone) {
    firstRenderDone = true;
    requestUpdate();
  } else if (!recentsLoaded && !recentsLoading) {
    recentsLoading = true;
    loadRecentCovers(metrics.homeCoverHeight);
  }
}

void HomeActivity::onSelectBook(const std::string& path) { activityManager.goToReader(path); }
