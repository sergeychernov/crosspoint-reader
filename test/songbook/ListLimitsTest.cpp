#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ListLimits.h"
#include "Setlist.h"
#include "SongbookTestSupport.h"

using songbook::clampUtf8;
using songbook::ListBudget;
using songbook::MAX_LABEL_BYTES;

namespace {

// "Я" is two bytes in UTF-8.
std::string cyrillic(const size_t letters) {
  std::string text;
  for (size_t i = 0; i < letters; ++i) text += "\xD0\xAF";
  return text;
}

}  // namespace

TEST(ListLimits, ClampKeepsShortTextAndWholeCharacters) {
  EXPECT_EQ(clampUtf8("short", 10), "short");
  EXPECT_EQ(clampUtf8("exactly10!", 10), "exactly10!");
  EXPECT_EQ(clampUtf8("eleven char", 10), "eleven cha");
  // An odd limit would split a two-byte letter; the half letter is dropped.
  EXPECT_EQ(clampUtf8(cyrillic(5), 7), cyrillic(3));
  EXPECT_EQ(clampUtf8(cyrillic(5), 8), cyrillic(4));
  EXPECT_EQ(clampUtf8("", 0), "");
}

TEST(ListLimits, BudgetStopsAtTheLimit) {
  ListBudget budget(100);
  EXPECT_TRUE(budget.take(60));
  EXPECT_TRUE(budget.take(40));
  EXPECT_FALSE(budget.exhausted());
  EXPECT_FALSE(budget.take(1));
  EXPECT_TRUE(budget.exhausted());
  // Once full, the list has ended; a smaller entry does not slip in after it.
  EXPECT_FALSE(budget.take(0));
  EXPECT_GT(ListBudget::stringCost(0), 0u);
}

TEST(ListLimits, ParserBoundsSongMetadata) {
  // The longest title a line can hold: 1000 bytes, within ChordProParser::MAX_LINE_BYTES.
  const std::string longTitle = cyrillic(500);
  const auto song = songbook_test::parse("{title: " + longTitle + "}\n{artist: " + longTitle + "}\n", true);
  EXPECT_EQ(song.meta.title, cyrillic(MAX_LABEL_BYTES / 2));
  EXPECT_EQ(song.meta.artist, cyrillic(MAX_LABEL_BYTES / 2));
}

TEST(ListLimits, SetlistTitleIsBounded) {
  // A title just under the 16 KB file limit, as in the reported case.
  const std::string title(12000, 'T');
  songbook::Setlist setlist;
  ASSERT_EQ(songbook::parseSetlist("{\"title\": \"" + title + "\", \"songs\": [\"a.cho\"]}", setlist),
            songbook::SetlistParseResult::Ok);
  EXPECT_EQ(setlist.title.size(), MAX_LABEL_BYTES);
  EXPECT_FALSE(setlist.truncated);
}

TEST(ListLimits, SetlistPathsStayWithinTheirBudget) {
  // 200 long paths would take about 50 KB; the budget keeps the first ones.
  const std::string folder(240, 'f');
  std::string json = "{\"songs\": [";
  for (int i = 0; i < 200; ++i) {
    if (i > 0) json += ",";
    json += "\"" + folder + "/" + std::to_string(i) + ".cho\"";
  }
  json += "]}";
  songbook::Setlist setlist;
  ASSERT_EQ(songbook::parseSetlist(json, setlist), songbook::SetlistParseResult::Ok);
  EXPECT_TRUE(setlist.truncated);
  ASSERT_FALSE(setlist.songs.empty());
  EXPECT_LT(setlist.songs.size(), 200u);
  size_t bytes = 0;
  for (const auto& path : setlist.songs) bytes += ListBudget::stringCost(path.size());
  EXPECT_LE(bytes, songbook::MAX_SONG_PATH_BYTES);
  // Order is kept: the songs that fit are the first ones.
  EXPECT_EQ(setlist.songs.front(), folder + "/0.cho");
}

TEST(ListLimits, OrdinarySetlistIsNotTruncated) {
  std::string json = "{\"songs\": [";
  for (int i = 0; i < 200; ++i) {
    if (i > 0) json += ",";
    json += "\"song-" + std::to_string(i) + ".cho\"";
  }
  json += "]}";
  songbook::Setlist setlist;
  ASSERT_EQ(songbook::parseSetlist(json, setlist), songbook::SetlistParseResult::Ok);
  EXPECT_FALSE(setlist.truncated);
  EXPECT_EQ(setlist.songs.size(), 200u);
}

TEST(ListLimits, TitleOnlyParseKeepsNoPaths) {
  songbook::Setlist setlist;
  EXPECT_EQ(songbook::parseSetlist(R"({"title": "T", "songs": ["/bad.cho", "a.cho"]})", setlist,
                                   songbook::SetlistRead::TitleOnly),
            songbook::SetlistParseResult::Ok);
  EXPECT_EQ(setlist.title, "T");
  EXPECT_TRUE(setlist.songs.empty());
  EXPECT_EQ(songbook::parseSetlist(R"({"songs": ["/bad.cho", "../x.cho"]})", setlist, songbook::SetlistRead::TitleOnly),
            songbook::SetlistParseResult::NoSongs);
}

TEST(ListLimits, OnlyTheFirstSongEntriesCount) {
  // Both modes look at the same first MAX_SETLIST_SONGS entries.
  std::string json = "{\"songs\": [";
  for (size_t i = 0; i < songbook::MAX_SETLIST_SONGS; ++i) json += "\"/bad.cho\",";
  json += "\"late.cho\"]}";
  songbook::Setlist setlist;
  EXPECT_EQ(songbook::parseSetlist(json, setlist, songbook::SetlistRead::TitleOnly),
            songbook::SetlistParseResult::NoSongs);
  EXPECT_EQ(songbook::parseSetlist(json, setlist), songbook::SetlistParseResult::NoSongs);
}

TEST(ListLimits, PathBudgetComesFromTheCallback) {
  // On the device the callback measures the heap; a low heap gives a small budget.
  songbook::Setlist setlist;
  const songbook::PathBudgetFn twoPaths = [](size_t) { return 2 * ListBudget::stringCost(5); };
  ASSERT_EQ(songbook::parseSetlist(R"({"songs": ["a.cho", "b.cho", "c.cho"]})", setlist,
                                   songbook::SetlistRead::WithSongs, twoPaths),
            songbook::SetlistParseResult::Ok);
  EXPECT_EQ(setlist.songs, (std::vector<std::string>{"a.cho", "b.cho"}));
  EXPECT_TRUE(setlist.truncated);

  // The callback is told the size of the vector it has to make room for.
  static size_t askedFor = 0;
  const songbook::PathBudgetFn none = [](const size_t vectorBytes) {
    askedFor = vectorBytes;
    return size_t{0};
  };
  EXPECT_EQ(songbook::parseSetlist(R"({"songs": ["a.cho", "/bad.cho", "b.cho"]})", setlist,
                                   songbook::SetlistRead::WithSongs, none),
            songbook::SetlistParseResult::OutOfMemory);
  EXPECT_EQ(askedFor, 2 * sizeof(std::string));
  EXPECT_TRUE(setlist.songs.empty());

  // A budget too small for the first path is a memory problem, not an empty setlist.
  const songbook::PathBudgetFn tooSmall = [](size_t) { return ListBudget::stringCost(5) - 1; };
  EXPECT_EQ(songbook::parseSetlist(R"({"songs": ["a.cho"]})", setlist, songbook::SetlistRead::WithSongs, tooSmall),
            songbook::SetlistParseResult::OutOfMemory);
}
