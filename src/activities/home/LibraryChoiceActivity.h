#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Shown by the Home grid's Library tile only when OPDS servers are configured (otherwise Library
// goes straight to FileBrowserActivity, unchanged from before the redesign). Folds the old
// top-level "OPDS Browser" grid slot into a 2-item chooser instead.
class LibraryChoiceActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

 public:
  explicit LibraryChoiceActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("LibraryChoice", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
