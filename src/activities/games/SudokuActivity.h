#pragma once

#include <cstdint>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Classic Sudoku. Two-region navigation: the 9x9 board (Left/Right/Up/Down move the cursor,
// Confirm on a non-given cell opens the number picker) and a 2x5 number-picker grid (1-9 +
// Erase; Confirm writes the digit and returns focus to the board). Win detection is done purely
// from Sudoku's rules (fully filled + every row/column/3x3 box has each digit once) — no stored
// solution needed.
class SudokuActivity final : public Activity {
  ButtonNavigator buttonNavigator;

  uint8_t board[81] = {};
  bool given[81] = {};
  int selectedRow = 0;
  int selectedCol = 0;
  int region = 0;  // 0 = board, 1 = number picker
  int pickerIndex = 0;
  bool solved = false;

  void resetBoard();
  void checkSolved();
  void applyPickerValue();

 public:
  static constexpr int boardSize = 9;
  static constexpr int pickerColumns = 5;
  static constexpr int pickerItemCount = 10;  // digits 1-9 + Erase

  explicit SudokuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Sudoku", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
