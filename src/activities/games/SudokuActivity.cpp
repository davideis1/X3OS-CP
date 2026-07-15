#include "SudokuActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/GridNavigator.h"

namespace {
// The well-known "easiest Sudoku" example grid (0 = blank). Hand-verified: no duplicate given
// digit in any row, column, or 3x3 box. More puzzles can be appended here later.
// clang-format off
constexpr uint8_t kPuzzles[][81] = {
    {5, 3, 0,  0, 7, 0,  0, 0, 0,
     6, 0, 0,  1, 9, 5,  0, 0, 0,
     0, 9, 8,  0, 0, 0,  0, 6, 0,

     8, 0, 0,  0, 6, 0,  0, 0, 3,
     4, 0, 0,  8, 0, 3,  0, 0, 1,
     7, 0, 0,  0, 2, 0,  0, 0, 6,

     0, 6, 0,  0, 0, 0,  2, 8, 0,
     0, 0, 0,  4, 1, 9,  0, 0, 5,
     0, 0, 0,  0, 8, 0,  0, 7, 9},
};
// clang-format on
}  // namespace

void SudokuActivity::resetBoard() {
  const auto& puzzle = kPuzzles[0];
  for (int i = 0; i < 81; ++i) {
    board[i] = puzzle[i];
    given[i] = puzzle[i] != 0;
  }
  solved = false;
  selectedRow = 0;
  selectedCol = 0;
  region = 0;
  pickerIndex = 0;
}

void SudokuActivity::onEnter() {
  Activity::onEnter();
  resetBoard();
  requestUpdate();
}

void SudokuActivity::checkSolved() {
  for (int i = 0; i < 81; ++i) {
    if (board[i] == 0) return;
  }

  for (int r = 0; r < boardSize; ++r) {
    bool seen[10] = {};
    for (int c = 0; c < boardSize; ++c) {
      const uint8_t v = board[r * boardSize + c];
      if (v < 1 || v > 9 || seen[v]) return;
      seen[v] = true;
    }
  }

  for (int c = 0; c < boardSize; ++c) {
    bool seen[10] = {};
    for (int r = 0; r < boardSize; ++r) {
      const uint8_t v = board[r * boardSize + c];
      if (seen[v]) return;
      seen[v] = true;
    }
  }

  for (int br = 0; br < 3; ++br) {
    for (int bc = 0; bc < 3; ++bc) {
      bool seen[10] = {};
      for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
          const uint8_t v = board[(br * 3 + r) * boardSize + (bc * 3 + c)];
          if (seen[v]) return;
          seen[v] = true;
        }
      }
    }
  }

  solved = true;
}

void SudokuActivity::applyPickerValue() {
  const int cellIndex = selectedRow * boardSize + selectedCol;
  if (pickerIndex == pickerItemCount - 1) {  // last cell = Erase
    board[cellIndex] = 0;
  } else {
    board[cellIndex] = static_cast<uint8_t>(pickerIndex + 1);
  }
  region = 0;
  checkSolved();
}

void SudokuActivity::loop() {
  using Button = MappedInputManager::Button;

  if (solved) {
    if (mappedInput.wasReleased(Button::Confirm)) {
      resetBoard();
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(Button::Back)) {
      onGoHome(HomeMenuItem::GAMES);
    }
    return;
  }

  if (mappedInput.wasReleased(Button::Back)) {
    if (region == 1) {
      region = 0;
      requestUpdate();
    } else {
      onGoHome(HomeMenuItem::GAMES);
    }
    return;
  }

  if (region == 0) {
    if (mappedInput.wasReleased(Button::Confirm)) {
      const int cellIndex = selectedRow * boardSize + selectedCol;
      if (!given[cellIndex]) {
        region = 1;
        pickerIndex = 0;
        requestUpdate();
      }
      return;
    }

    buttonNavigator.onPressAndContinuous({Button::Left}, [this] {
      selectedCol = (selectedCol - 1 + boardSize) % boardSize;
      requestUpdate();
    });
    buttonNavigator.onPressAndContinuous({Button::Right}, [this] {
      selectedCol = (selectedCol + 1) % boardSize;
      requestUpdate();
    });
    buttonNavigator.onPressAndContinuous({Button::Up}, [this] {
      selectedRow = (selectedRow - 1 + boardSize) % boardSize;
      requestUpdate();
    });
    buttonNavigator.onPressAndContinuous({Button::Down}, [this] {
      selectedRow = (selectedRow + 1) % boardSize;
      requestUpdate();
    });
    return;
  }

  // region == 1: number picker
  if (mappedInput.wasReleased(Button::Confirm)) {
    applyPickerValue();
    requestUpdate();
    return;
  }

  buttonNavigator.onPressAndContinuous({Button::Left}, [this] {
    GridNavigator::moveLeft(pickerIndex, pickerColumns, pickerItemCount);
    requestUpdate();
  });
  buttonNavigator.onPressAndContinuous({Button::Right}, [this] {
    GridNavigator::moveRight(pickerIndex, pickerColumns, pickerItemCount);
    requestUpdate();
  });
  buttonNavigator.onPressAndContinuous({Button::Up}, [this] {
    GridNavigator::moveUp(pickerIndex, pickerColumns, pickerItemCount);
    requestUpdate();
  });
  buttonNavigator.onPressAndContinuous({Button::Down}, [this] {
    GridNavigator::moveDown(pickerIndex, pickerColumns, pickerItemCount);
    requestUpdate();
  });
}

void SudokuActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SUDOKU));

  const int boardTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int pickerAreaHeight = metrics.keyboardKeyHeight * 2 + metrics.keyboardKeySpacing;
  const int reservedBelow = metrics.verticalSpacing * 2 + pickerAreaHeight + metrics.buttonHintsHeight + metrics.verticalSpacing;
  const int availableHeight = pageHeight - boardTop - reservedBelow;
  const int availableWidth = pageWidth - metrics.contentSidePadding * 2;
  const int boardSizePx = std::min(availableWidth, availableHeight);
  const int cellSize = std::max(1, boardSizePx / boardSize);
  const int boardLeft = (pageWidth - cellSize * boardSize) / 2;

  for (int i = 0; i <= boardSize; ++i) {
    const int lineWidth = (i % 3 == 0) ? 3 : 1;
    renderer.drawLine(boardLeft, boardTop + i * cellSize, boardLeft + cellSize * boardSize, boardTop + i * cellSize,
                      lineWidth, true);
    renderer.drawLine(boardLeft + i * cellSize, boardTop, boardLeft + i * cellSize, boardTop + cellSize * boardSize,
                      lineWidth, true);
  }

  for (int r = 0; r < boardSize; ++r) {
    for (int c = 0; c < boardSize; ++c) {
      const int cellIndex = r * boardSize + c;
      const int cellX = boardLeft + c * cellSize;
      const int cellY = boardTop + r * cellSize;
      const bool selected = region == 0 && r == selectedRow && c == selectedCol;
      if (selected) {
        renderer.fillRect(cellX + 1, cellY + 1, cellSize - 2, cellSize - 2, true);
      }

      const uint8_t value = board[cellIndex];
      if (value != 0) {
        const char buf[2] = {static_cast<char>('0' + value), '\0'};
        const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, buf);
        const int textX = cellX + (cellSize - textWidth) / 2;
        const int textY = cellY + (cellSize - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
        renderer.drawText(UI_12_FONT_ID, textX, textY, buf, !selected,
                          given[cellIndex] ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
      }
    }
  }

  const int pickerTop = boardTop + cellSize * boardSize + metrics.verticalSpacing * 2;

  if (solved) {
    renderer.drawCenteredText(UI_12_FONT_ID, pickerTop + 10, tr(STR_SOLVED), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, pickerTop + 10 + renderer.getLineHeight(UI_12_FONT_ID) + 8,
                              tr(STR_NEW_GAME), true);
  } else {
    const int keyHeight = metrics.keyboardKeyHeight;
    const int keySpacing = metrics.keyboardKeySpacing;
    const int keyboardWidth = pageWidth * metrics.keyboardWidthPercent / 100;
    const int keyWidth = (keyboardWidth - (pickerColumns - 1) * keySpacing) / pickerColumns;
    const int leftMargin = (pageWidth - (pickerColumns * keyWidth + (pickerColumns - 1) * keySpacing)) / 2;

    for (int i = 0; i < pickerItemCount; ++i) {
      const int row = i / pickerColumns;
      const int col = i % pickerColumns;
      const int keyX = leftMargin + col * (keyWidth + keySpacing);
      const int keyY = pickerTop + row * (keyHeight + keySpacing);
      const bool selected = region == 1 && i == pickerIndex;

      if (i == pickerItemCount - 1) {
        GUI.drawKeyboardKey(renderer, Rect{keyX, keyY, keyWidth, keyHeight}, tr(STR_ERASE), selected);
      } else {
        const char buf[2] = {static_cast<char>('1' + i), '\0'};
        GUI.drawKeyboardKey(renderer, Rect{keyX, keyY, keyWidth, keyHeight}, buf, selected);
      }
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  GUI.drawSideButtonHints(renderer, tr(STR_DIR_UP), tr(STR_DIR_DOWN));

  renderer.displayBuffer();
}
