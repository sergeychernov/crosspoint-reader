#pragma once

#include <vector>

#include "activities/UiListActivity.h"

// List screen shared by the setlist picker and a setlist's song list: rows are
// built once on entry; emptyMessage replaces the list when there are none.
class SongbookListActivity : public UiListActivity {
 protected:
  SongbookListActivity(const char* name, GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity(name, renderer, mappedInput) {}

  int listCount() const override { return static_cast<int>(rowItems.size()); }
  void buildScreen(UiScreen& screen) override;
  // Appends the inactive row that says the list did not fit in memory.
  void addCutRow();
  // Button navigation steps over inactive rows; isHeader keeps only touch off a row.
  void navigateButtons() override;
  // First active row from index, moving by step (wraps); index itself if none is active.
  int activeRowFrom(int index, int step) const;
  // Moves the selection off an inactive row after the rows are built.
  void selectActiveRow();

  std::vector<freeink::ui::ListItem> rowItems;
  const char* emptyMessage = nullptr;
};
