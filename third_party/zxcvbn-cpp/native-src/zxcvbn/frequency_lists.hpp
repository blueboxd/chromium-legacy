#ifndef __ZXCVBN__FREQUENCY_LISTS_HPP
#define __ZXCVBN__FREQUENCY_LISTS_HPP

#include <zxcvbn/frequency_lists_common.hpp>

#include <unordered_map>

#include <cstdint>

#include "base/strings/string_piece.h"

namespace zxcvbn {

enum class DictionaryTag {
  ENGLISH_WIKIPEDIA,
  FEMALE_NAMES,
  MALE_NAMES,
  PASSWORDS,
  SURNAMES,
  US_TV_AND_FILM,
  USER_INPUTS
};

}

namespace std {

template<>
struct hash<zxcvbn::DictionaryTag> {
  std::size_t operator()(const zxcvbn::DictionaryTag & v) const {
    return static_cast<std::size_t>(v);
  }
};

}

namespace zxcvbn {

using RankedDicts = std::unordered_map<DictionaryTag, const RankedDict &>;

bool ParseRankedDictionary(DictionaryTag tag, base::StringPiece str);

RankedDicts convert_to_ranked_dicts(std::unordered_map<DictionaryTag, RankedDict> & ranked_dicts);
RankedDicts default_ranked_dicts();

}

#endif
