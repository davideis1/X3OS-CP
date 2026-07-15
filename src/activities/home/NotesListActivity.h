#pragma once

#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Notes are plain .txt files under /Notes on the SD card (not a PersistableStore blob — this way
// they're visible/manageable through File Transfer/the web UI too, same as library books). The
// row list is the directory listing plus one trailing virtual "+ New Note" row.
class NotesListActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;
  int confirmingDelete = 0;  // 0 = off, 1 = display, 2 = confirm (mirrors EpubReaderBookmarksActivity)

  std::vector<std::string> files;  // filenames only (e.g. "Shopping list.txt"), not full paths
  std::unique_ptr<char[]> fileNameBuffer;

  void loadFiles();
  int rowCount() const;
  bool isNewNoteRow(int index) const;
  std::string pathFor(int index) const;
  std::string displayTitleFor(int index) const;

  void openNote(int index);
  void startNewNote();
  void promptNoteBody(const std::string& path, const std::string& initialBody);

 public:
  explicit NotesListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("NotesList", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
