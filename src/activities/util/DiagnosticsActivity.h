#pragma once

#include "activities/Activity.h"

// Read-only system info screen (battery, free heap, uptime, SD usage, firmware version). No
// persistence and no auto-refresh loop — e-ink refreshes are expensive, so a single snapshot
// taken at onEnter() is enough for a diagnostics screen.
class DiagnosticsActivity final : public Activity {
 public:
  explicit DiagnosticsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Diagnostics", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
