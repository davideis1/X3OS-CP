#include "NotesListActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <string_view>

#include "MappedInputManager.h"
#include "activities/ActivityResult.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr size_t NAME_BUFFER_SIZE = 500;
constexpr const char* NOTES_DIR = "/Notes";
constexpr int ENTER_DELETE_MODE_MS = 700;
constexpr int DELETE_MODE_OFF = 0;
constexpr int DELETE_MODE_DISPLAY = 1;
constexpr int DELETE_MODE_CONFIRM = 2;
}  // namespace

void NotesListActivity::loadFiles() {
  files.clear();

  auto root = Storage.open(NOTES_DIR);
  if (!root || !root.isDirectory()) {
    return;
  }
  root.rewindDirectory();

  if (!fileNameBuffer) {
    LOG_ERR("NOTES", "fileNameBuffer not allocated");
    root.close();
    return;
  }

  for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(fileNameBuffer.get(), NAME_BUFFER_SIZE);
    if (!file.isDirectory()) {
      std::string_view filename{fileNameBuffer.get()};
      if (FsHelpers::hasTxtExtension(filename)) {
        files.emplace_back(filename);
      }
    }
  }
  root.close();
  FsHelpers::sortFileList(files);
}

int NotesListActivity::rowCount() const { return static_cast<int>(files.size()) + 1; }

bool NotesListActivity::isNewNoteRow(int index) const { return index == static_cast<int>(files.size()); }

std::string NotesListActivity::pathFor(int index) const {
  return std::string(NOTES_DIR) + "/" + files[static_cast<size_t>(index)];
}

std::string NotesListActivity::displayTitleFor(int index) const {
  const std::string& name = files[static_cast<size_t>(index)];
  constexpr std::string_view suffix = ".txt";
  if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
    return name.substr(0, name.size() - suffix.size());
  }
  return name;
}

void NotesListActivity::onEnter() {
  Activity::onEnter();
  Storage.mkdir(NOTES_DIR);
  fileNameBuffer = makeUniqueNoThrow<char[]>(NAME_BUFFER_SIZE);
  if (!fileNameBuffer) {
    LOG_ERR("NOTES", "OOM: fileNameBuffer (%zu bytes)", NAME_BUFFER_SIZE);
  }
  loadFiles();
  selectorIndex = 0;
  confirmingDelete = DELETE_MODE_OFF;
  requestUpdate();
}

void NotesListActivity::onExit() {
  Activity::onExit();
  fileNameBuffer.reset();
}

void NotesListActivity::promptNoteBody(const std::string& path, const std::string& initialBody) {
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_NOTES), initialBody, 500),
      [this, path](const ActivityResult& result) {
        if (!result.isCancelled) {
          const auto& kb = std::get<KeyboardResult>(result.data);
          if (!kb.text.empty()) {
            Storage.writeFile(path.c_str(), String(kb.text.c_str()));
            loadFiles();
          }
        }
        requestUpdate();
      });
}

void NotesListActivity::openNote(int index) {
  const std::string path = pathFor(index);
  const String content = Storage.readFile(path.c_str());
  promptNoteBody(path, std::string(content.c_str()));
}

void NotesListActivity::startNewNote() {
  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_NOTE_TITLE), "", 40),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             requestUpdate();
                             return;
                           }
                           const auto& kb = std::get<KeyboardResult>(result.data);
                           if (kb.text.empty()) {
                             requestUpdate();
                             return;
                           }

                           char sanitized[64];
                           FsHelpers::sanitizePathComponentForFat32(kb.text.c_str(), sanitized, sizeof(sanitized));
                           if (sanitized[0] == '\0') {
                             requestUpdate();
                             return;
                           }

                           const std::string base = std::string(NOTES_DIR) + "/" + sanitized;
                           std::string path = base + ".txt";
                           if (Storage.exists(path.c_str())) {
                             bool found = false;
                             for (int suffix = 2; suffix <= 20; ++suffix) {
                               std::string candidate = base + "-" + std::to_string(suffix) + ".txt";
                               if (!Storage.exists(candidate.c_str())) {
                                 path = candidate;
                                 found = true;
                                 break;
                               }
                             }
                             if (!found) {
                               requestUpdate();
                               return;
                             }
                           }

                           promptNoteBody(path, "");
                         });
}

void NotesListActivity::loop() {
  using Button = MappedInputManager::Button;

  if (confirmingDelete >= DELETE_MODE_DISPLAY) {
    if (mappedInput.wasReleased(Button::Confirm)) {
      if (confirmingDelete == DELETE_MODE_DISPLAY) {
        confirmingDelete = DELETE_MODE_CONFIRM;
        requestUpdate();
        return;
      }
      Storage.remove(pathFor(selectorIndex).c_str());
      loadFiles();
      if (selectorIndex >= rowCount() - 1 && selectorIndex > 0) {
        selectorIndex--;
      }
      confirmingDelete = DELETE_MODE_OFF;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(Button::Back)) {
      confirmingDelete = DELETE_MODE_OFF;
      requestUpdate();
    }
    return;
  }

  if (mappedInput.wasReleased(Button::Back)) {
    onGoHome(HomeMenuItem::NOTES);
    return;
  }

  if (mappedInput.wasReleased(Button::Confirm)) {
    if (isNewNoteRow(selectorIndex)) {
      startNewNote();
    } else {
      openNote(selectorIndex);
    }
    return;
  }

  if (!isNewNoteRow(selectorIndex) && mappedInput.isPressed(Button::Confirm) &&
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

void NotesListActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_NOTES));

  const int count = static_cast<int>(files.size());
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  if (confirmingDelete >= DELETE_MODE_DISPLAY) {
    GUI.drawHelpText(renderer, Rect{0, pageHeight / 2 - 120, pageWidth, 60}, tr(STR_CONFIRM_DELETE_ITEM));
    GUI.drawList(renderer, Rect{0, pageHeight / 2, pageWidth, 60}, 1, 0,
                 [this](int) { return displayTitleFor(selectorIndex); });
  } else {
    const auto rowTitle = [this, count](int index) -> std::string {
      if (index == count) return std::string("+ ") + tr(STR_NEW_NOTE);
      return displayTitleFor(index);
    };
    GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, count + 1, selectorIndex, rowTitle);

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
