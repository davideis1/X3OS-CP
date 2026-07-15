#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Games hub opened from Home. Sudoku is real; Klondike still routes to ComingSoonActivity.
class GamesListActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

 public:
  static constexpr int itemCount = 2;

  explicit GamesListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GamesList", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
