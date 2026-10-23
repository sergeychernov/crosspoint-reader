#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ListLimits.h"

namespace songbook {

// Ordered song files, relative to /songbook/songs/.
struct Setlist {
  std::string title;
  std::vector<std::string> songs;
  // True when songs past the path budget were left out.
  bool truncated = false;
};

// OutOfMemory: the path budget could not hold even the first usable song.
enum class SetlistParseResult { Ok, InvalidJson, NoSongs, OutOfMemory };

// TitleOnly keeps no song paths; the result only tells whether one is usable.
enum class SetlistRead : uint8_t { TitleOnly, WithSongs };

// Path budget for a parsed document, given the bytes its song vector will take.
// It runs after the JSON document is built, so it can measure the live heap.
using PathBudgetFn = size_t (*)(size_t vectorBytes);

// Hard bound for a hand-edited or corrupt setlist.
constexpr size_t MAX_SETLIST_SONGS = 200;

// {"title": "...", "songs": ["a.cho", "folder/b.chordpro"]}. Unsafe or
// non-string entries are skipped; NoSongs means none were usable. The title is
// clamped to MAX_LABEL_BYTES. Song paths are kept within the budget from
// pathBudget (ListBudget::stringCost each), or MAX_SONG_PATH_BYTES without one.
SetlistParseResult parseSetlist(std::string_view json, Setlist& out, SetlistRead read = SetlistRead::WithSongs,
                                PathBudgetFn pathBudget = nullptr);

// Relative path that cannot leave the songs folder: no leading '/', no "..",
// no empty segments, no backslashes or control bytes.
bool isSafeSongPath(std::string_view path);

bool hasSongExtension(std::string_view name);

}  // namespace songbook
