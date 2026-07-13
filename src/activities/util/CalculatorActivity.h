#pragma once

#include <string>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Classic four-function running-accumulator calculator (no expression parser/precedence —
// applies each operator immediately against the running accumulator, like real calculator
// hardware). Reached from the Tools folder.
class CalculatorActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

  double accumulator = 0.0;
  char pendingOp = 0;  // 0 = none
  std::string currentEntry;
  bool showingError = false;

  static constexpr size_t MAX_ENTRY_LENGTH = 12;

  double parseCurrentEntry() const;
  void applyPendingOp();
  void pressKey(int index);

 public:
  static constexpr int columns = 4;
  static constexpr int itemCount = 20;

  explicit CalculatorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Calculator", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
