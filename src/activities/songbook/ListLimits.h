#pragma once

#include <cstddef>
#include <string_view>

namespace songbook {

// Stored length of a title, artist, or setlist name: 64 Cyrillic or 128 Latin
// letters. The screen shows far less, so a longer value only costs RAM.
constexpr size_t MAX_LABEL_BYTES = 128;
// Text budget of the setlist picker and of one song list. A low heap lowers it.
constexpr size_t MAX_LIST_TEXT_BYTES = 32 * 1024;
// Budget for the song paths of one setlist, or of All songs.
constexpr size_t MAX_SONG_PATH_BYTES = 16 * 1024;

// Longest prefix of text within maxBytes that ends on a UTF-8 character boundary.
std::string_view clampUtf8(std::string_view text, size_t maxBytes);

// Running byte budget for the strings of one list. Once an entry does not fit,
// the list stops there and shows that it was cut.
class ListBudget {
 public:
  explicit ListBudget(const size_t limit) : limit(limit) {}

  // False, and nothing is taken, when bytes would pass the limit.
  bool take(size_t bytes);
  bool exhausted() const { return full; }

  // Heap bytes a string of exactly length bytes may need: the terminator, the
  // allocator header, heap poisoning, and 4-byte rounding. Strings short
  // enough to stay inline need none, but are counted too.
  static constexpr size_t stringCost(const size_t length) { return length + 1 + STRING_OVERHEAD_BYTES; }

 private:
  static constexpr size_t STRING_OVERHEAD_BYTES = 20;

  size_t limit;
  size_t used = 0;
  bool full = false;
};

}  // namespace songbook
