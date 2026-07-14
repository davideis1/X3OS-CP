#include "UnitConverterActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>
#include <cstdlib>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/GridNavigator.h"

namespace {
struct FactorUnit {
  StrId label;
  double factor;  // relative to the category's base unit
};

constexpr FactorUnit kLengthUnits[4] = {
    {StrId::STR_UNIT_METERS, 1.0},
    {StrId::STR_UNIT_KILOMETERS, 1000.0},
    {StrId::STR_UNIT_MILES, 1609.34},
    {StrId::STR_UNIT_FEET, 0.3048},
};
constexpr FactorUnit kWeightUnits[4] = {
    {StrId::STR_UNIT_GRAMS, 1.0},
    {StrId::STR_UNIT_KILOGRAMS, 1000.0},
    {StrId::STR_UNIT_POUNDS, 453.592},
    {StrId::STR_UNIT_OUNCES, 28.3495},
};
constexpr StrId kTempUnits[3] = {StrId::STR_UNIT_CELSIUS, StrId::STR_UNIT_FAHRENHEIT, StrId::STR_UNIT_KELVIN};
constexpr StrId kCategoryLabels[3] = {StrId::STR_UNIT_CAT_LENGTH, StrId::STR_UNIT_CAT_WEIGHT, StrId::STR_UNIT_CAT_TEMP};

int unitCount(UnitCategory category) {
  switch (category) {
    case UnitCategory::LENGTH:
      return 4;
    case UnitCategory::WEIGHT:
      return 4;
    case UnitCategory::TEMP:
      return 3;
  }
  return 4;
}

StrId unitLabel(UnitCategory category, int index) {
  switch (category) {
    case UnitCategory::LENGTH:
      return kLengthUnits[index].label;
    case UnitCategory::WEIGHT:
      return kWeightUnits[index].label;
    case UnitCategory::TEMP:
      return kTempUnits[index];
  }
  return StrId::STR_UNIT_METERS;
}

// Celsius <-> {Celsius=0, Fahrenheit=1, Kelvin=2}
double toCelsius(int unitIndex, double v) {
  switch (unitIndex) {
    case 1:
      return (v - 32.0) * 5.0 / 9.0;
    case 2:
      return v - 273.15;
    default:
      return v;
  }
}
double fromCelsius(int unitIndex, double c) {
  switch (unitIndex) {
    case 1:
      return c * 9.0 / 5.0 + 32.0;
    case 2:
      return c + 273.15;
    default:
      return c;
  }
}

double convertValue(UnitCategory category, int fromIndex, int toIndex, double value) {
  switch (category) {
    case UnitCategory::LENGTH:
      return value * kLengthUnits[fromIndex].factor / kLengthUnits[toIndex].factor;
    case UnitCategory::WEIGHT:
      return value * kWeightUnits[fromIndex].factor / kWeightUnits[toIndex].factor;
    case UnitCategory::TEMP:
      return fromCelsius(toIndex, toCelsius(fromIndex, value));
  }
  return value;
}

enum class ConvKeyType { Digit, Decimal, Clear };
struct ConvKey {
  ConvKeyType type;
  char value;
  const char* label;
};

// clang-format off
constexpr ConvKey kConvKeys[UnitConverterActivity::keypadItemCount] = {
    {ConvKeyType::Digit, '1', "1"}, {ConvKeyType::Digit, '2', "2"}, {ConvKeyType::Digit, '3', "3"}, {ConvKeyType::Clear, 0, "C"},
    {ConvKeyType::Digit, '4', "4"}, {ConvKeyType::Digit, '5', "5"}, {ConvKeyType::Digit, '6', "6"}, {ConvKeyType::Decimal, 0, "."},
    {ConvKeyType::Digit, '7', "7"}, {ConvKeyType::Digit, '8', "8"}, {ConvKeyType::Digit, '9', "9"}, {ConvKeyType::Digit, '0', "0"},
};
// clang-format on
}  // namespace

void UnitConverterActivity::onEnter() {
  Activity::onEnter();
  region = 0;
  keypadIndex = 0;
  category = UnitCategory::LENGTH;
  fromUnitIndex = 0;
  toUnitIndex = 1;
  valueBuffer.clear();
  requestUpdate();
}

double UnitConverterActivity::convert(double value) const {
  return convertValue(category, fromUnitIndex, toUnitIndex, value);
}

void UnitConverterActivity::cycleCategory() {
  category = static_cast<UnitCategory>((static_cast<int>(category) + 1) % 3);
  fromUnitIndex = 0;
  toUnitIndex = 1;
}

void UnitConverterActivity::cycleFromUnit() { fromUnitIndex = (fromUnitIndex + 1) % unitCount(category); }

void UnitConverterActivity::cycleToUnit() { toUnitIndex = (toUnitIndex + 1) % unitCount(category); }

void UnitConverterActivity::pressKeypadKey(int index) {
  const ConvKey& key = kConvKeys[index];
  switch (key.type) {
    case ConvKeyType::Digit:
      if (valueBuffer.size() < MAX_VALUE_LENGTH) valueBuffer += key.value;
      break;
    case ConvKeyType::Decimal:
      if (valueBuffer.find('.') == std::string::npos && valueBuffer.size() < MAX_VALUE_LENGTH) {
        if (valueBuffer.empty()) valueBuffer = "0";
        valueBuffer += '.';
      }
      break;
    case ConvKeyType::Clear:
      valueBuffer.clear();
      break;
  }
}

void UnitConverterActivity::loop() {
  using Button = MappedInputManager::Button;

  if (mappedInput.wasReleased(Button::Back)) {
    onGoHome(HomeMenuItem::TOOLS);
    return;
  }

  if (region < 3) {
    if (mappedInput.wasReleased(Button::Confirm)) {
      if (region == 0) {
        cycleCategory();
      } else if (region == 1) {
        cycleFromUnit();
      } else {
        cycleToUnit();
      }
      requestUpdate();
      return;
    }
    buttonNavigator.onPressAndContinuous({Button::Up}, [this] {
      if (region > 0) {
        region--;
        requestUpdate();
      }
    });
    buttonNavigator.onPressAndContinuous({Button::Down}, [this] {
      if (region < 2) {
        region++;
      } else {
        region = 3;
        keypadIndex = 0;
      }
      requestUpdate();
    });
    return;
  }

  if (mappedInput.wasReleased(Button::Confirm)) {
    pressKeypadKey(keypadIndex);
    requestUpdate();
    return;
  }

  buttonNavigator.onPressAndContinuous({Button::Left}, [this] {
    GridNavigator::moveLeft(keypadIndex, keypadColumns, keypadItemCount);
    requestUpdate();
  });
  buttonNavigator.onPressAndContinuous({Button::Right}, [this] {
    GridNavigator::moveRight(keypadIndex, keypadColumns, keypadItemCount);
    requestUpdate();
  });
  buttonNavigator.onPressAndContinuous({Button::Up}, [this] {
    if (keypadIndex < keypadColumns) {
      region = 2;
    } else {
      GridNavigator::moveUp(keypadIndex, keypadColumns, keypadItemCount);
    }
    requestUpdate();
  });
  buttonNavigator.onPressAndContinuous({Button::Down}, [this] {
    GridNavigator::moveDown(keypadIndex, keypadColumns, keypadItemCount);
    requestUpdate();
  });
}

void UnitConverterActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CONVERT));

  const int sidePad = metrics.contentSidePadding;
  constexpr int rowHeight = 50;
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  // Category row
  {
    const bool selected = region == 0;
    if (selected) {
      renderer.fillRect(sidePad, y, pageWidth - sidePad * 2, rowHeight);
    } else {
      renderer.drawRect(sidePad, y, pageWidth - sidePad * 2, rowHeight);
    }
    renderer.drawCenteredText(UI_12_FONT_ID, y + (rowHeight - lineHeight) / 2,
                              I18N.get(kCategoryLabels[static_cast<int>(category)]), !selected);
  }
  y += rowHeight + metrics.verticalSpacing;

  const double typedValue = valueBuffer.empty() ? 0.0 : strtod(valueBuffer.c_str(), nullptr);
  const double convertedValue = convert(typedValue);

  // From-unit row
  {
    const bool selected = region == 1;
    if (selected) {
      renderer.fillRect(sidePad, y, pageWidth - sidePad * 2, rowHeight);
    } else {
      renderer.drawRect(sidePad, y, pageWidth - sidePad * 2, rowHeight);
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %s", I18N.get(unitLabel(category, fromUnitIndex)),
             valueBuffer.empty() ? "0" : valueBuffer.c_str());
    renderer.drawText(UI_12_FONT_ID, sidePad + 12, y + (rowHeight - lineHeight) / 2, buf, !selected);
  }
  y += rowHeight + metrics.verticalSpacing;

  // To-unit row (live-computed)
  {
    const bool selected = region == 2;
    if (selected) {
      renderer.fillRect(sidePad, y, pageWidth - sidePad * 2, rowHeight);
    } else {
      renderer.drawRect(sidePad, y, pageWidth - sidePad * 2, rowHeight);
    }
    char valBuf[32];
    snprintf(valBuf, sizeof(valBuf), "%g", convertedValue);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %s", I18N.get(unitLabel(category, toUnitIndex)), valBuf);
    renderer.drawText(UI_12_FONT_ID, sidePad + 12, y + (rowHeight - lineHeight) / 2, buf, !selected,
                      EpdFontFamily::BOLD);
  }
  y += rowHeight + metrics.verticalSpacing * 2;

  const int keyHeight = metrics.keyboardKeyHeight;
  const int keySpacing = metrics.keyboardKeySpacing;
  const int keyboardWidth = pageWidth * metrics.keyboardWidthPercent / 100;
  const int keyWidth = (keyboardWidth - (keypadColumns - 1) * keySpacing) / keypadColumns;
  const int leftMargin = (pageWidth - (keypadColumns * keyWidth + (keypadColumns - 1) * keySpacing)) / 2;

  for (int i = 0; i < keypadItemCount; ++i) {
    const int row = i / keypadColumns;
    const int col = i % keypadColumns;
    const int keyX = leftMargin + col * (keyWidth + keySpacing);
    const int keyY = y + row * (keyHeight + keySpacing);
    const bool selected = region == 3 && i == keypadIndex;
    GUI.drawKeyboardKey(renderer, Rect{keyX, keyY, keyWidth, keyHeight}, kConvKeys[i].label, selected);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  GUI.drawSideButtonHints(renderer, tr(STR_DIR_UP), tr(STR_DIR_DOWN));

  renderer.displayBuffer();
}
