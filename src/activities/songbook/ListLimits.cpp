#include "ListLimits.h"

#include <Utf8.h>

namespace songbook {

std::string_view clampUtf8(const std::string_view text, const size_t maxBytes) {
  if (text.size() <= maxBytes) return text;
  const int length = utf8SafeTruncateBuffer(text.data(), static_cast<int>(maxBytes));
  return text.substr(0, static_cast<size_t>(length));
}

bool ListBudget::take(const size_t bytes) {
  if (full || bytes > limit - used) {
    full = true;
    return false;
  }
  used += bytes;
  return true;
}

}  // namespace songbook
