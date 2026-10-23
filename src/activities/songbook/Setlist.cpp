#include "Setlist.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <cstring>

#include "ListLimits.h"

namespace songbook {

namespace {

bool endsWithIgnoreCase(const std::string_view text, const std::string_view suffix) {
  if (text.size() < suffix.size()) return false;
  const std::string_view tail = text.substr(text.size() - suffix.size());
  for (size_t i = 0; i < suffix.size(); ++i) {
    const char c = tail[i] >= 'A' && tail[i] <= 'Z' ? static_cast<char>(tail[i] - 'A' + 'a') : tail[i];
    if (c != suffix[i]) return false;
  }
  return true;
}

}  // namespace

bool hasSongExtension(const std::string_view name) {
  return endsWithIgnoreCase(name, ".cho") || endsWithIgnoreCase(name, ".chordpro") ||
         endsWithIgnoreCase(name, ".chopro") || endsWithIgnoreCase(name, ".crd");
}

bool isSafeSongPath(const std::string_view path) {
  if (path.empty() || path.size() > 255 || path.front() == '/') return false;
  size_t segmentStart = 0;
  for (size_t i = 0; i <= path.size(); ++i) {
    if (i < path.size()) {
      const auto c = static_cast<unsigned char>(path[i]);
      if (c < 0x20 || c == '\\') return false;
      if (c != '/') continue;
    }
    const std::string_view segment = path.substr(segmentStart, i - segmentStart);
    if (segment.empty() || segment == "." || segment == "..") return false;
    segmentStart = i + 1;
  }
  return true;
}

SetlistParseResult parseSetlist(const std::string_view json, Setlist& out, const SetlistRead read,
                                const PathBudgetFn pathBudget) {
  out.title.clear();
  out.songs.clear();
  out.truncated = false;
  JsonDocument doc;
  if (deserializeJson(doc, json.data(), json.size()) != DeserializationError::Ok || !doc.is<JsonObjectConst>()) {
    return SetlistParseResult::InvalidJson;
  }
  if (const char* title = doc["title"].as<const char*>()) out.title = std::string(clampUtf8(title, MAX_LABEL_BYTES));
  const JsonArrayConst songs = doc["songs"].as<JsonArrayConst>();
  if (songs.isNull()) return SetlistParseResult::InvalidJson;

  // Counted first, so the vector is reserved once and the budget sees its size.
  // A loop, not std::count_if: ArduinoJson's array iterators have no iterator_category.
  size_t usable = 0;
  size_t seen = 0;
  for (const JsonVariantConst entry : songs) {
    if (seen++ >= MAX_SETLIST_SONGS) break;
    const char* path = entry.as<const char*>();
    if (path && isSafeSongPath(path)) ++usable;
  }
  if (usable == 0) return SetlistParseResult::NoSongs;
  if (read == SetlistRead::TitleOnly) return SetlistParseResult::Ok;

  const size_t vectorBytes = usable * sizeof(std::string);
  const size_t budgetBytes = pathBudget ? pathBudget(vectorBytes) : MAX_SONG_PATH_BYTES;
  if (budgetBytes == 0) return SetlistParseResult::OutOfMemory;
  out.songs.reserve(usable);
  ListBudget budget(budgetBytes);
  seen = 0;
  for (const JsonVariantConst entry : songs) {
    if (seen++ >= MAX_SETLIST_SONGS) break;
    const char* path = entry.as<const char*>();
    if (!path || !isSafeSongPath(path)) continue;
    if (!budget.take(ListBudget::stringCost(strlen(path)))) {
      out.truncated = true;
      break;
    }
    out.songs.emplace_back(path);
  }
  // Usable songs exist, so an empty list here means the budget could not hold the first one.
  return out.songs.empty() ? SetlistParseResult::OutOfMemory : SetlistParseResult::Ok;
}

}  // namespace songbook
