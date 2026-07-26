#include "versionComparator.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

namespace {
struct ParsedVersion {
  uint32_t major = 0;
  uint32_t minor = 0;
  uint64_t suffixRank = 0;
  bool valid = false;
};

bool parseNumber(const char *&cursor, uint32_t &value) {
  if (cursor == nullptr || !isdigit(static_cast<unsigned char>(*cursor))) return false;

  value = 0;
  while (isdigit(static_cast<unsigned char>(*cursor))) {
    const uint32_t digit = static_cast<uint32_t>(*cursor - '0');
    if (value > (UINT32_MAX - digit) / 10U) return false;
    value = value * 10U + digit;
    cursor++;
  }
  return true;
}

ParsedVersion parseVersion(const char *version) {
  ParsedVersion parsed;
  if (version == nullptr) return parsed;

  const char *cursor = version;
  while (isspace(static_cast<unsigned char>(*cursor))) cursor++;
  if (*cursor == 'V' || *cursor == 'v') cursor++;
  if (!parseNumber(cursor, parsed.major) || *cursor++ != '.' || !parseNumber(cursor, parsed.minor)) {
    return parsed;
  }

  while (isalpha(static_cast<unsigned char>(*cursor))) {
    const uint8_t digit = static_cast<uint8_t>(tolower(static_cast<unsigned char>(*cursor)) - 'a' + 1);
    if (digit < 1 || digit > 26) return parsed;
    if (parsed.suffixRank > (UINT64_MAX - digit) / 26ULL) return parsed;
    parsed.suffixRank = parsed.suffixRank * 26ULL + digit;
    cursor++;
  }

  while (isspace(static_cast<unsigned char>(*cursor))) cursor++;
  parsed.valid = *cursor == '\0';
  return parsed;
}

int compareUnsigned(uint64_t left, uint64_t right) {
  if (left == right) return 0;
  return left > right ? 1 : -1;
}

int compareCaseInsensitive(const char *left, const char *right) {
  if (left == nullptr) left = "";
  if (right == nullptr) right = "";
  while (*left != '\0' && *right != '\0') {
    const int leftChar = tolower(static_cast<unsigned char>(*left));
    const int rightChar = tolower(static_cast<unsigned char>(*right));
    if (leftChar != rightChar) return leftChar > rightChar ? 1 : -1;
    left++;
    right++;
  }
  return compareUnsigned(strlen(left), strlen(right));
}
}  // namespace

int compareFirmwareVersionStrings(const char *leftVersion, const char *rightVersion) {
  const ParsedVersion left = parseVersion(leftVersion);
  const ParsedVersion right = parseVersion(rightVersion);
  if (!left.valid || !right.valid) return compareCaseInsensitive(leftVersion, rightVersion);

  int comparison = compareUnsigned(left.major, right.major);
  if (comparison != 0) return comparison;
  comparison = compareUnsigned(left.minor, right.minor);
  if (comparison != 0) return comparison;
  return compareUnsigned(left.suffixRank, right.suffixRank);
}
