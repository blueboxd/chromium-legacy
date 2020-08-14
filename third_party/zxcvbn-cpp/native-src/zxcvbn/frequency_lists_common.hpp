#ifndef __ZXCVBN__FREQUENCY_LISTS_COMMON_HPP
#define __ZXCVBN__FREQUENCY_LISTS_COMMON_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"

namespace zxcvbn {

using rank_t = std::size_t;
using RankedDict = base::flat_map<std::string, rank_t>;

template<class T>
RankedDict build_ranked_dict(const T & ordered_list) {
  std::vector<RankedDict::value_type> items;
  items.reserve(ordered_list.size());
  rank_t idx = 1; // rank starts at 1, not 0
  for (const auto & word : ordered_list) {
    items.emplace_back(word, idx);
    idx += 1;
  }
  return RankedDict(std::move(items));
}

}

#endif
