#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// "Tools" grid opened from Home: Calculator, Unit Converter, Diagnostics, Weather. Sudoku and
// Klondike live under the Games hub (Home > Games) only, not here — they were removed from this
// grid to avoid two navigation paths to the same activity.
class ToolsFolderActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

 public:
  static constexpr int columns = 2;
  static constexpr int itemCount = 4;

  explicit ToolsFolderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ToolsFolder", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
