#include "CalculatorActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/GridNavigator.h"

namespace {
enum class CalcKeyType { Digit, Decimal, Backspace, Clear, Operator, Equals, Blank };
struct CalcKey {
  CalcKeyType type;
  char value;
  const char* label;
};

// clang-format off
constexpr CalcKey kKeys[CalculatorActivity::itemCount] = {
    {CalcKeyType::Digit, '7', "7"}, {CalcKeyType::Digit, '8', "8"}, {CalcKeyType::Digit, '9', "9"}, {CalcKeyType::Operator, '/', "/"},
    {CalcKeyType::Digit, '4', "4"}, {CalcKeyType::Digit, '5', "5"}, {CalcKeyType::Digit, '6', "6"}, {CalcKeyType::Operator, '*', "*"},
    {CalcKeyType::Digit, '1', "1"}, {CalcKeyType::Digit, '2', "2"}, {CalcKeyType::Digit, '3', "3"}, {CalcKeyType::Operator, '-', "-"},
    {CalcKeyType::Digit, '0', "0"}, {CalcKeyType::Decimal, 0, "."}, {CalcKeyType::Backspace, 0, nullptr}, {CalcKeyType::Operator, '+', "+"},
    {CalcKeyType::Clear, 0, "C"},   {CalcKeyType::Blank, 0, nullptr}, {CalcKeyType::Blank, 0, nullptr}, {CalcKeyType::Equals, 0, "="},
};
// clang-format on
}  // namespace

void CalculatorActivity::onEnter() {
  Activity::onEnter();
  accumulator = 0.0;
  pendingOp = 0;
  currentEntry.clear();
  showingError = false;
  selectedIndex = 0;
  requestUpdate();
}

double CalculatorActivity::parseCurrentEntry() const {
  if (currentEntry.empty()) return 0.0;
  return strtod(currentEntry.c_str(), nullptr);
}

void CalculatorActivity::applyPendingOp() {
  if (pendingOp == 0) {
    accumulator = parseCurrentEntry();
    return;
  }
  const double operand = parseCurrentEntry();
  double result = accumulator;
  switch (pendingOp) {
    case '+':
      result = accumulator + operand;
      break;
    case '-':
      result = accumulator - operand;
      break;
    case '*':
      result = accumulator * operand;
      break;
    case '/':
      if (operand == 0.0) {
        showingError = true;
        return;
      }
      result = accumulator / operand;
      break;
    default:
      break;
  }
  if (!std::isfinite(result)) {
    showingError = true;
    return;
  }
  accumulator = result;
}

void CalculatorActivity::pressKey(int index) {
  const CalcKey& key = kKeys[index];

  if (showingError) {
    accumulator = 0.0;
    pendingOp = 0;
    currentEntry.clear();
    showingError = false;
    if (key.type != CalcKeyType::Digit) return;
  }

  switch (key.type) {
    case CalcKeyType::Blank:
      return;
    case CalcKeyType::Digit:
      if (currentEntry.size() < MAX_ENTRY_LENGTH) currentEntry += key.value;
      return;
    case CalcKeyType::Decimal:
      if (currentEntry.find('.') == std::string::npos && currentEntry.size() < MAX_ENTRY_LENGTH) {
        if (currentEntry.empty()) currentEntry = "0";
        currentEntry += '.';
      }
      return;
    case CalcKeyType::Backspace:
      if (!currentEntry.empty()) currentEntry.pop_back();
      return;
    case CalcKeyType::Clear:
      accumulator = 0.0;
      pendingOp = 0;
      currentEntry.clear();
      return;
    case CalcKeyType::Operator:
      applyPendingOp();
      if (!showingError) {
        pendingOp = key.value;
        currentEntry.clear();
      }
      return;
    case CalcKeyType::Equals:
      applyPendingOp();
      pendingOp = 0;
      currentEntry.clear();
      return;
  }
}

void CalculatorActivity::loop() {
  using Button = MappedInputManager::Button;

  if (mappedInput.wasReleased(Button::Back)) {
    onGoHome(HomeMenuItem::TOOLS);
    return;
  }

  if (mappedInput.wasReleased(Button::Confirm)) {
    pressKey(selectedIndex);
    requestUpdate();
    return;
  }

  buttonNavigator.onPressAndContinuous({Button::Left}, [this] {
    GridNavigator::moveLeft(selectedIndex, columns, itemCount);
    requestUpdate();
  });
  buttonNavigator.onPressAndContinuous({Button::Right}, [this] {
    GridNavigator::moveRight(selectedIndex, columns, itemCount);
    requestUpdate();
  });
  buttonNavigator.onPressAndContinuous({Button::Up}, [this] {
    GridNavigator::moveUp(selectedIndex, columns, itemCount);
    requestUpdate();
  });
  buttonNavigator.onPressAndContinuous({Button::Down}, [this] {
    GridNavigator::moveDown(selectedIndex, columns, itemCount);
    requestUpdate();
  });
}

void CalculatorActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CALCULATOR));

  std::string displayText;
  if (showingError) {
    displayText = tr(STR_CALC_ERROR);
  } else if (!currentEntry.empty()) {
    displayText = currentEntry;
  } else {
    char buf[32];
    snprintf(buf, sizeof(buf), "%g", accumulator);
    displayText = buf;
  }

  const int sidePad = metrics.contentSidePadding;
  const int displayY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  constexpr int displayHeight = 60;
  renderer.drawRect(sidePad, displayY, pageWidth - sidePad * 2, displayHeight, 2, true);
  const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, displayText.c_str(), EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, pageWidth - sidePad - 12 - textWidth,
                    displayY + (displayHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2, displayText.c_str(), true,
                    EpdFontFamily::BOLD);

  const int keyHeight = metrics.keyboardKeyHeight;
  const int keySpacing = metrics.keyboardKeySpacing;
  const int keyboardWidth = pageWidth * metrics.keyboardWidthPercent / 100;
  const int keyWidth = (keyboardWidth - (columns - 1) * keySpacing) / columns;
  const int leftMargin = (pageWidth - (columns * keyWidth + (columns - 1) * keySpacing)) / 2;
  const int keyboardStartY = displayY + displayHeight + metrics.verticalSpacing * 2;

  for (int i = 0; i < itemCount; ++i) {
    const int row = i / columns;
    const int col = i % columns;
    const int keyX = leftMargin + col * (keyWidth + keySpacing);
    const int keyY = keyboardStartY + row * (keyHeight + keySpacing);
    const bool selected = i == selectedIndex;
    const CalcKey& key = kKeys[i];

    if (key.type == CalcKeyType::Blank) {
      if (selected) {
        renderer.drawRect(keyX, keyY, keyWidth, keyHeight);
      }
      continue;
    }

    const auto keyType = key.type == CalcKeyType::Backspace ? KeyboardKeyType::Del : KeyboardKeyType::Normal;
    GUI.drawKeyboardKey(renderer, Rect{keyX, keyY, keyWidth, keyHeight}, key.label, selected, nullptr, keyType);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  GUI.drawSideButtonHints(renderer, tr(STR_DIR_UP), tr(STR_DIR_DOWN));

  renderer.displayBuffer();
}
