#include <zxcvbn/frequency_lists.hpp>

#include <unordered_map>
#include <utility>

#include "base/no_destructor.h"

namespace zxcvbn {

namespace {

std::unordered_map<DictionaryTag, RankedDict>& ranked_dicts() {
  static base::NoDestructor<std::unordered_map<DictionaryTag, RankedDict>>
      ranked_dicts;
  return *ranked_dicts;
}

}  // namespace

void SetRankedDicts(std::unordered_map<DictionaryTag, RankedDict> dicts) {
  ranked_dicts() = std::move(dicts);
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
