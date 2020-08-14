#ifndef __ZXCVBN__FREQUENCY_LISTS_HPP
#define __ZXCVBN__FREQUENCY_LISTS_HPP

#include <zxcvbn/frequency_lists_common.hpp>

#include <cstdint>

#include "base/strings/string_piece.h"
#include "base/containers/flat_map.h"

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

namespace zxcvbn {

using RankedDicts = base::flat_map<DictionaryTag, const RankedDict*>;

void SetRankedDicts(base::flat_map<DictionaryTag, RankedDict> dicts);

RankedDicts convert_to_ranked_dicts(base::flat_map<DictionaryTag, RankedDict> & ranked_dicts);
RankedDicts default_ranked_dicts();

}

#endif
