#pragma once

#include <I18n.h>

#include "activities/Activity.h"

// Generic placeholder screen for grid tiles that don't have a real feature behind them yet
// (Games, To-Do, Notes, Weather, and every Tools-folder entry). Title-only + "Coming soon" body
// so every stub tile shares one Activity instead of needing its own class.
class ComingSoonActivity final : public Activity {
  const StrId title;

 public:
  explicit ComingSoonActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, StrId title)
      : Activity("ComingSoon", renderer, mappedInput), title(title) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
