#include <zxcvbn/frequency_lists.hpp>

#include <unordered_map>

#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/string_piece.h"

namespace zxcvbn {

namespace {

std::unordered_map<DictionaryTag, RankedDict>& ranked_dicts() {
  static base::NoDestructor<std::unordered_map<DictionaryTag, RankedDict>>
      ranked_dicts;
  return *ranked_dicts;
}

}

bool ParseRankedDictionary(DictionaryTag tag, base::StringPiece str) {
  RankedDict& dict = ranked_dicts()[tag];
  if (!dict.empty())
    return false;

  dict = build_ranked_dict(base::SplitStringPiece(
      str, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));
  return true;
}

RankedDicts convert_to_ranked_dicts(std::unordered_map<DictionaryTag, RankedDict> & ranked_dicts) {
  RankedDicts build;

  for (const auto & item : ranked_dicts) {
    build.insert(item);
  }

  return build;
}

RankedDicts default_ranked_dicts() {
  return convert_to_ranked_dicts(ranked_dicts());
}

}
