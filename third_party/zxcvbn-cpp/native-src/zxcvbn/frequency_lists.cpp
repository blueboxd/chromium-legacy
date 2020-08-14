#include <zxcvbn/frequency_lists.hpp>

#include <utility>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"

namespace zxcvbn {

namespace {

base::flat_map<DictionaryTag, RankedDict>& ranked_dicts() {
  static base::NoDestructor<base::flat_map<DictionaryTag, RankedDict>>
      ranked_dicts;
  return *ranked_dicts;
}

}  // namespace

void SetRankedDicts(base::flat_map<DictionaryTag, RankedDict> dicts) {
  ranked_dicts() = std::move(dicts);
}

RankedDicts convert_to_ranked_dicts(
    base::flat_map<DictionaryTag, RankedDict>& ranked_dicts) {
  RankedDicts build;

  for (const auto & item : ranked_dicts) {
    build.emplace(item.first, &item.second);
  }

  return build;
}

RankedDicts default_ranked_dicts() {
  return convert_to_ranked_dicts(ranked_dicts());
}

}
