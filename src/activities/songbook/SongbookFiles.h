#pragma once

#include <string>
#include <vector>

#include "Setlist.h"
#include "Song.h"

namespace songbook {

constexpr char ROOT_DIR[] = "/songbook";
constexpr char SONGS_DIR[] = "/songbook/songs";
constexpr char SETLISTS_DIR[] = "/songbook/setlists";
// Real songs are a few KB; the cap bounds the parsed pool for a wrong file.
constexpr size_t MAX_SONG_FILE_BYTES = 64 * 1024;
constexpr size_t MAX_SETLIST_FILE_BYTES = 16 * 1024;
constexpr int MAX_LISTED_FILES = 200;

enum class SongLoadResult { Ok, Missing, TooLarge, Empty, OutOfMemory };

enum class SetlistStatus { Ok, Unreadable, NoSongs };

// How much of a list fit in memory. A truncated list holds the entries that fit:
// a setlist's first songs, or files in the order the card returns them.
enum class ListResult { Complete, Truncated, OutOfMemory };

struct SetlistFile {
  std::string file;  // name inside SETLISTS_DIR
  std::string title;
  SetlistStatus status = SetlistStatus::Unreadable;
};

bool songbookFolderExists();
// relativePath is under SONGS_DIR. The title falls back to the file name.
SongLoadResult loadSong(const std::string& relativePath, Song& song, bool metadataOnly);
// Song files directly in SONGS_DIR (the first MAX_LISTED_FILES entries), sorted
// by name, within the path budget.
ListResult listSongFiles(std::vector<std::string>& out);
bool hasSongFiles();
// Setlist files in SETLISTS_DIR with their titles, sorted by file name, within
// the list budget. rowBytes is what the caller adds per entry for its rows.
ListResult listSetlists(std::vector<SetlistFile>& out, size_t rowBytes);
// Text budget, up to textLimit, for a list whose fixed entries take entryBytes,
// leaving headroom for the screens that open from it. Lower than textLimit when
// the heap is low; 0 when it does not fit.
size_t listTextBudget(size_t entryBytes, size_t textLimit);
// False when the file cannot be read; result tells a parse failure apart.
// The path budget comes from the live heap once the file is parsed.
bool loadSetlist(const std::string& file, Setlist& out, SetlistParseResult& result,
                 SetlistRead read = SetlistRead::WithSongs);

}  // namespace songbook
