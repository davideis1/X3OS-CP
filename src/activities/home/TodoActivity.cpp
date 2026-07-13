#include "TodoActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <string>

#include "MappedInputManager.h"
#include "TodoStore.h"
#include "activities/ActivityResult.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int ENTER_DELETE_MODE_MS = 700;
constexpr int DELETE_MODE_OFF = 0;
constexpr int DELETE_MODE_DISPLAY = 1;
constexpr int DELETE_MODE_CONFIRM = 2;
}  // namespace

int TodoActivity::rowCount() const { return static_cast<int>(TODO_STORE.getCount()) + 1; }

bool TodoActivity::isAddRow(int index) const { return index == static_cast<int>(TODO_STORE.getCount()); }

void TodoActivity::onEnter() {
  Activity::onEnter();
  selectorIndex = 0;
  confirmingDelete = DELETE_MODE_OFF;
  requestUpdate();
}

void TodoActivity::openAddDialog() {
  auto handler = [this](const ActivityResult& result) {
    if (!result.isCancelled) {
      const auto& kb = std::get<KeyboardResult>(result.data);
      if (!kb.text.empty() && TODO_STORE.addItem(kb.text)) {
        selectorIndex = static_cast<int>(TODO_STORE.getCount()) - 1;
      }
    }
    requestUpdate();
  };
  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_ADD_REMINDER), "", 80),
                         handler);
}

void TodoActivity::loop() {
  if (confirmingDelete >= DELETE_MODE_DISPLAY) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (confirmingDelete == DELETE_MODE_DISPLAY) {
        confirmingDelete = DELETE_MODE_CONFIRM;
        requestUpdate();
        return;
      }
      TODO_STORE.removeItem(static_cast<size_t>(selectorIndex));
      if (selectorIndex >= rowCount() - 1 && selectorIndex > 0) {
        selectorIndex--;
      }
      confirmingDelete = DELETE_MODE_OFF;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      confirmingDelete = DELETE_MODE_OFF;
      requestUpdate();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome(HomeMenuItem::TODO);
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (isAddRow(selectorIndex)) {
      openAddDialog();
    } else {
      TODO_STORE.toggleItem(static_cast<size_t>(selectorIndex));
      requestUpdate();
    }
    return;
  }

  if (!isAddRow(selectorIndex) && mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
      mappedInput.getHeldTime() > ENTER_DELETE_MODE_MS) {
    confirmingDelete = DELETE_MODE_DISPLAY;
    requestUpdate();
    return;
  }

  const int count = rowCount();
  buttonNavigator.onNext([this, count] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, count);
    requestUpdate();
  });
  buttonNavigator.onPrevious([this, count] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, count);
    requestUpdate();
  });
}

void TodoActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_TODO));

  const auto& items = TODO_STORE.getItems();
  const int count = static_cast<int>(items.size());

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  if (confirmingDelete >= DELETE_MODE_DISPLAY) {
    GUI.drawHelpText(renderer, Rect{0, pageHeight / 2 - 120, pageWidth, 60}, tr(STR_CONFIRM_DELETE_ITEM));
    GUI.drawList(renderer, Rect{0, pageHeight / 2, pageWidth, 60}, 1, 0,
                 [&items, this](int) { return items[static_cast<size_t>(selectorIndex)].text; });
  } else {
    const auto rowTitle = [&items, count](int index) -> std::string {
      if (index == count) return std::string("+ ") + tr(STR_ADD_REMINDER);
      return items[static_cast<size_t>(index)].text;
    };
    const auto rowValue = [&items, count](int index) -> std::string {
      if (index == count || !items[static_cast<size_t>(index)].done) return "";
      return "\xE2\x9C\x93";  // checkmark
    };
    const auto rowDimmed = [&items, count](int index) {
      return index != count && items[static_cast<size_t>(index)].done;
    };

    GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, count + 1, selectorIndex, rowTitle, nullptr,
                 nullptr, rowValue, false, rowDimmed);

    if (count > 0) {
      GUI.drawHelpText(renderer, Rect{0, pageHeight - metrics.buttonHintsHeight - 60, pageWidth, 60},
                       tr(STR_HOLD_TO_DELETE));
    }
  }

  const auto backLabel = confirmingDelete >= DELETE_MODE_DISPLAY ? tr(STR_CANCEL) : tr(STR_BACK);
  const auto confirmLabel = confirmingDelete >= DELETE_MODE_DISPLAY ? tr(STR_DELETE) : tr(STR_SELECT);
  const auto labels = mappedInput.mapLabels(backLabel, confirmLabel, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
