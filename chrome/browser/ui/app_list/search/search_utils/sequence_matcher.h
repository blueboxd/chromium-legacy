// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_UTILS_SEQUENCE_MATCHER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_UTILS_SEQUENCE_MATCHER_H_

#include <string>

#include "base/logging.h"
#include "base/macros.h"

namespace app_list {

// Performs the calculation of similarity level between 2 strings. This class's
// functionality is inspired by python's difflib.SequenceMatcher library.
// (https://docs.python.org/2/library/difflib.html#difflib.SequenceMatcher)
class SequenceMatcher {
 public:
  SequenceMatcher(const std::string& first_string,
                  const std::string& second_string)
      : first_string_(first_string), second_string_(second_string) {
    DCHECK(!first_string_.empty() || !second_string_.empty());
  }
  ~SequenceMatcher() = default;

  // Calculates similarity ratio of |first_string_| and |second_string_|.
  // |use_edit_distance| is the option to use edit distance or block matching
  // as the algorithm.
  double Ratio(bool use_edit_distance = true);
  // Calculates the Damerau–Levenshtein distance between |first_string_| and
  // |second_string_|.
  // See https://en.wikipedia.org/wiki/Damerau–Levenshtein_distance for more
  // details.
  int EditDistance();

 private:
  const std::string first_string_;
  const std::string second_string_;
  double edit_distance_ratio_ = -1.0;
  double block_matching_ratio_ = -1.0;
  int edit_distance_ = -1;
  DISALLOW_COPY_AND_ASSIGN(SequenceMatcher);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_UTILS_SEQUENCE_MATCHER_H_
