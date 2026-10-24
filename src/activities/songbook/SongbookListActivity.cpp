#include "SongbookListActivity.h"

#include <I18n.h>

#include "components/UITheme.h"

namespace fui = freeink::ui;

int SongbookListActivity::activeRowFrom(const int index, const int step) const {
  const int count = static_cast<int>(rowItems.size());
  if (count == 0) return index;
  int row = ((index % count) + count) % count;
  for (int tries = 0; tries < count; ++tries) {
    if (rowItems[row].enabled) return row;
    row = (row + step + count) % count;
  }
  return index;
}

void SongbookListActivity::selectActiveRow() {
  if (nav.selected >= 0 && nav.selected < static_cast<int>(rowItems.size()) && !rowItems[nav.selected].enabled) {
    nav.selected = activeRowFrom(nav.selected, 1);
  }
}

void SongbookListActivity::navigateButtons() {
  buttonNavigator.onNextPress(
      [this] { moveSelectionTo(activeRowFrom(ButtonNavigator::nextIndex(nav.selected, listCount()), 1)); });
  buttonNavigator.onPreviousPress(
      [this] { moveSelectionTo(activeRowFrom(ButtonNavigator::previousIndex(nav.selected, listCount()), -1)); });
  buttonNavigator.onNextContinuous([this] {
    moveSelectionTo(activeRowFrom(ButtonNavigator::nextPageIndex(nav.selected, listCount(), nav.inputPageRows()), 1));
  });
  buttonNavigator.onPreviousContinuous([this] {
    moveSelectionTo(
        activeRowFrom(ButtonNavigator::previousPageIndex(nav.selected, listCount(), nav.inputPageRows()), -1));
  });
}

void SongbookListActivity::addCutRow() {
  fui::ListItem item;
  item.label = tr(STR_SONGBOOK_LIST_CUT);
  // Touch skips a header row; navigateButtons() and the enabled check keep buttons off it.
  item.isHeader = true;
  item.enabled = false;
  item.actionValue = static_cast<int16_t>(rowItems.size());
  rowItems.push_back(item);
}

void SongbookListActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  // Content: the safe area minus the header band drawChrome paints.
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (rowItems.empty()) {
    if (emptyMessage) screen.centeredText(emptyMessage, screen.theme().bodyText);
    return;
  }
  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  syncListViewport(screen, props);
  screen.list(props);
}
