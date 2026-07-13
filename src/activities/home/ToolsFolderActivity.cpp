#include "ToolsFolderActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <vector>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/GridNavigator.h"

namespace {
struct ToolDef {
  StrId label;
  UIIcon icon;
};
constexpr ToolDef kTools[ToolsFolderActivity::itemCount] = {
    {StrId::STR_CALCULATOR, UIIcon::None},  {StrId::STR_CONVERT, UIIcon::None},
    {StrId::STR_SUDOKU, UIIcon::None},      {StrId::STR_KLONDIKE, UIIcon::None},
    {StrId::STR_DIAGNOSTICS, UIIcon::None}, {StrId::STR_WEATHER, UIIcon::Weather},
};
}  // namespace

void ToolsFolderActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  requestUpdate();
}

void ToolsFolderActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome(HomeMenuItem::TOOLS);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activityManager.goToComingSoon(kTools[selectedIndex].label);
    return;
  }

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left}, [this] {
    GridNavigator::moveLeft(selectedIndex, columns, itemCount);
    requestUpdate();
  });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right}, [this] {
    GridNavigator::moveRight(selectedIndex, columns, itemCount);
    requestUpdate();
  });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up}, [this] {
    GridNavigator::moveUp(selectedIndex, columns, itemCount);
    requestUpdate();
  });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down}, [this] {
    GridNavigator::moveDown(selectedIndex, columns, itemCount);
    requestUpdate();
  });
}

void ToolsFolderActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_TOOLS));

  std::vector<GridTile> tiles;
  tiles.reserve(itemCount);
  for (const auto& tool : kTools) {
    tiles.push_back(GridTile{tool.icon, I18N.get(tool.label)});
  }

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  GUI.drawIconGrid(
      renderer, Rect{metrics.contentSidePadding, contentTop, pageWidth - metrics.contentSidePadding * 2, contentHeight},
      columns, tiles, selectedIndex);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
