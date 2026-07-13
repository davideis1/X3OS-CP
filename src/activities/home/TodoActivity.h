#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// A small persisted checklist (TODO_STORE, see TodoStore.h). The row list is
// TODO_STORE.getCount() items plus one trailing virtual "+ Add reminder" row.
class TodoActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;
  int confirmingDelete = 0;  // 0 = off, 1 = display, 2 = confirm (mirrors EpubReaderBookmarksActivity)

  int rowCount() const;
  bool isAddRow(int index) const;
  void openAddDialog();

 public:
  explicit TodoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Todo", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
