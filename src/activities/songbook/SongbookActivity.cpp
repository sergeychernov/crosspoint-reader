#include "SongbookActivity.h"

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <utility>

#include "SetlistActivity.h"

namespace fui = freeink::ui;

const char* SongbookActivity::headerTitle() const { return tr(STR_SONGBOOK); }

void SongbookActivity::onEnter() {
  SongbookListActivity::onEnter();
  rowItems.clear();
  if (!songbook::songbookFolderExists()) {
    emptyMessage = tr(STR_SONGBOOK_NO_FOLDER);
    requestUpdate();
    return;
  }
  const auto listed = songbook::listSetlists(setlists, sizeof(fui::ListItem));
  if (listed == songbook::ListResult::OutOfMemory) {
    emptyMessage = tr(STR_SONGBOOK_OUT_OF_MEMORY);
    requestUpdate();
    return;
  }
  hasSongs = songbook::hasSongFiles();

  // Setlists, the cut notice, and All songs.
  rowItems.reserve(setlists.size() + 2);
  for (const auto& setlist : setlists) {
    fui::ListItem item;
    item.label = setlist.title.c_str();
    switch (setlist.status) {
      case songbook::SetlistStatus::Ok:
        break;
      case songbook::SetlistStatus::Unreadable:
        item.value = tr(STR_SONGBOOK_SETLIST_INVALID);
        break;
      case songbook::SetlistStatus::NoSongs:
        item.value = tr(STR_SONGBOOK_SETLIST_EMPTY);
        break;
    }
    item.actionValue = static_cast<int16_t>(rowItems.size());
    rowItems.push_back(item);
  }
  // The notice closes the setlists it refers to; All songs stays last.
  if (listed == songbook::ListResult::Truncated) addCutRow();
  if (hasSongs) {
    fui::ListItem item;
    item.label = tr(STR_SONGBOOK_ALL_SONGS);
    item.actionValue = static_cast<int16_t>(rowItems.size());
    rowItems.push_back(item);
  }
  emptyMessage = tr(STR_SONGBOOK_NOTHING_FOUND);
  selectActiveRow();
  requestUpdate();
}

void SongbookActivity::onExit() {
  SongbookListActivity::onExit();
  rowItems.clear();
  setlists.clear();
}

void SongbookActivity::activateIndex(const int index) {
  if (index < 0 || index >= static_cast<int>(rowItems.size()) || !rowItems[index].enabled) return;
  app.clearTapFlash();
  nav.selected = index;

  songbook::Setlist setlist;
  const char* problem = nullptr;
  // Rows: setlists, then the inactive cut notice if any, then All songs.
  if (index < static_cast<int>(setlists.size())) {
    songbook::SetlistParseResult result = songbook::SetlistParseResult::InvalidJson;
    if (!songbook::loadSetlist(setlists[index].file, setlist, result))
      result = songbook::SetlistParseResult::InvalidJson;
    switch (result) {
      case songbook::SetlistParseResult::Ok:
        break;
      case songbook::SetlistParseResult::InvalidJson:
        problem = tr(STR_SONGBOOK_SETLIST_INVALID);
        break;
      case songbook::SetlistParseResult::NoSongs:
        problem = tr(STR_SONGBOOK_SETLIST_EMPTY);
        break;
      case songbook::SetlistParseResult::OutOfMemory:
        problem = tr(STR_SONGBOOK_OUT_OF_MEMORY);
        break;
    }
    if (setlist.title.empty()) setlist.title = setlists[index].title;
  } else {
    switch (songbook::listSongFiles(setlist.songs)) {
      case songbook::ListResult::Complete:
        break;
      case songbook::ListResult::Truncated:
        setlist.truncated = true;
        break;
      case songbook::ListResult::OutOfMemory:
        problem = tr(STR_SONGBOOK_OUT_OF_MEMORY);
        break;
    }
    setlist.title = tr(STR_SONGBOOK_ALL_SONGS);
  }

  auto activity = makeUniqueNoThrow<SetlistActivity>(renderer, mappedInput, std::move(setlist), problem);
  if (!activity) {
    LOG_ERR("SONG", "OOM: setlist activity");
    return;
  }
  activityManager.pushActivity(std::move(activity));
}
