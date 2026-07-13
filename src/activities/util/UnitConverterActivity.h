#pragma once

#include <string>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

enum class UnitCategory { LENGTH, WEIGHT, TEMP };

// Length/Weight/Temperature converter. Three "cycle rows" (category, from-unit, to-unit) sit
// above a numeric keypad for typing the value — Up/Down move between the rows and the keypad,
// Confirm on a row cycles its value, Confirm on a keypad cell types/clears a digit.
class UnitConverterActivity final : public Activity {
  ButtonNavigator buttonNavigator;

  // 0 = category row, 1 = from-unit row, 2 = to-unit row, 3 = keypad (focus tracked by keypadIndex)
  int region = 0;
  int keypadIndex = 0;

  UnitCategory category = UnitCategory::LENGTH;
  int fromUnitIndex = 0;
  int toUnitIndex = 1;
  std::string valueBuffer;

  static constexpr size_t MAX_VALUE_LENGTH = 12;

  double convert(double value) const;
  void pressKeypadKey(int index);
  void cycleCategory();
  void cycleFromUnit();
  void cycleToUnit();

 public:
  static constexpr int keypadColumns = 4;
  static constexpr int keypadItemCount = 12;

  explicit UnitConverterActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("UnitConverter", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
