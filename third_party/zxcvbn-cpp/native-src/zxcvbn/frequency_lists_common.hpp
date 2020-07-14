#ifndef __ZXCVBN__FREQUENCY_LISTS_COMMON_HPP
#define __ZXCVBN__FREQUENCY_LISTS_COMMON_HPP

#include <string>
#include <unordered_map>
#include <utility>

#include <cstdint>

namespace zxcvbn {

using rank_t = std::size_t;
using RankedDict = std::unordered_map<std::string, rank_t>;

template<class T>
RankedDict build_ranked_dict(const T & ordered_list) {
  RankedDict result;
  rank_t idx = 1; // rank starts at 1, not 0
  for (const auto & word : ordered_list) {
    result.insert(std::make_pair(word, idx));
    idx += 1;
  }
  return result;
}

}

#endif
