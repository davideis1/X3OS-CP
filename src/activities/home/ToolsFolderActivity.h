#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// "Tools" grid opened from Home. Every entry currently routes to ComingSoonActivity — none of
// these tools exist in the firmware yet (see the Home redesign plan). This screen only needs to
// exist so the Home grid has somewhere real to send the Tools tile.
class ToolsFolderActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

 public:
  static constexpr int columns = 3;
  static constexpr int itemCount = 6;

  explicit ToolsFolderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ToolsFolder", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
