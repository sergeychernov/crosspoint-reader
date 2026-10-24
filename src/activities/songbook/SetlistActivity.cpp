#include "SetlistActivity.h"

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>
#include <variant>

#include "ListLimits.h"
#include "SongActivity.h"
#include "SongbookFiles.h"

namespace fui = freeink::ui;

void SetlistActivity::onEnter() {
  SongbookListActivity::onEnter();
  emptyMessage = problem ? problem : tr(STR_SONGBOOK_SETLIST_EMPTY);
  rowItems.clear();
  if (problem) {
    requestUpdate();
    return;
  }

  const size_t count = setlist.songs.size();
  // A label, an artist, and a row per song, plus one row for the cut notice.
  const size_t entryBytes = count * (2 * sizeof(std::string) + sizeof(fui::ListItem)) + sizeof(fui::ListItem);
  const size_t budgetBytes = songbook::listTextBudget(entryBytes, songbook::MAX_LIST_TEXT_BYTES);
  if (count > 0 && budgetBytes == 0) {
    LOG_ERR("SONG", "OOM: list of %u songs", static_cast<unsigned>(count));
    emptyMessage = tr(STR_SONGBOOK_OUT_OF_MEMORY);
    requestUpdate();
    return;
  }
  // Rows point into these strings, so both are sized before any is filled.
  labels.reserve(count);
  artists.reserve(count);
  rowItems.reserve(count + 1);
  songbook::ListBudget budget(budgetBytes);
  songbook::Song header;
  for (size_t i = 0; i < count; ++i) {
    const auto result = songbook::loadSong(setlist.songs[i], header, true);
    // The parser bounds the title and artist; the file name fallback is bounded here.
    const std::string_view title = songbook::clampUtf8(
        header.meta.title.empty() ? std::string_view(setlist.songs[i]) : std::string_view(header.meta.title),
        songbook::MAX_LABEL_BYTES);
    char number[8];
    const int prefix = snprintf(number, sizeof(number), "%u. ", static_cast<unsigned>(i + 1));
    const size_t labelLength = static_cast<size_t>(prefix) + title.size();
    if (!budget.take(songbook::ListBudget::stringCost(labelLength) +
                     songbook::ListBudget::stringCost(header.meta.artist.size()))) {
      break;
    }
    // Allocated once at its exact length, then filled: "NNN. " and the title.
    labels.emplace_back(labelLength, '\0');
    memcpy(labels.back().data(), number, static_cast<size_t>(prefix));
    memcpy(labels.back().data() + prefix, title.data(), title.size());
    artists.emplace_back(header.meta.artist);

    fui::ListItem item;
    item.label = labels.back().c_str();
    if (!artists.back().empty()) item.subtitle = artists.back().c_str();
    switch (result) {
      case songbook::SongLoadResult::Ok:
        break;
      case songbook::SongLoadResult::TooLarge:
        item.value = tr(STR_SONGBOOK_FILE_TOO_LARGE);
        break;
      case songbook::SongLoadResult::Missing:
      case songbook::SongLoadResult::Empty:
      case songbook::SongLoadResult::OutOfMemory:
        item.value = tr(STR_SONGBOOK_FILE_MISSING);
        break;
    }
    item.actionValue = static_cast<int16_t>(i);
    rowItems.push_back(item);
  }
  // The song screen steps through setlist.songs, so it ends where the list does.
  if (budget.exhausted()) setlist.songs.resize(labels.size());
  if (budget.exhausted() || setlist.truncated) addCutRow();
  selectActiveRow();
  requestUpdate();
}

void SetlistActivity::onExit() {
  SongbookListActivity::onExit();
  rowItems.clear();
  labels.clear();
  artists.clear();
}

void SetlistActivity::activateIndex(const int index) {
  if (index < 0 || index >= static_cast<int>(rowItems.size()) || !rowItems[index].enabled) return;
  app.clearTapFlash();
  nav.selected = index;
  auto activity = makeUniqueNoThrow<SongActivity>(renderer, mappedInput, setlist, static_cast<size_t>(index));
  if (!activity) {
    LOG_ERR("SONG", "OOM: song activity");
    return;
  }
  // The song screen reports the song it ended on so the list follows it.
  startActivityForResult(std::move(activity), [this](const ActivityResult& result) {
    if (const auto* page = std::get_if<PageResult>(&result.data)) {
      if (page->page < rowItems.size()) moveSelectionTo(static_cast<int>(page->page));
    }
  });
}
