// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_utils/sequence_matcher.h"

#include <algorithm>

namespace app_list {

int SequenceMatcher::EditDistance() {
  // If edit distance is already calculated
  if (edit_distance_ >= 0) {
    return edit_distance_;
  }

  const int len_first = first_string_.size();
  const int len_second = second_string_.size();
  if (len_first == 0 || len_second == 0) {
    edit_distance_ = std::max(len_first, len_second);
    return edit_distance_;
  }

  // Memory for the dynamic programming:
  // dp[i + 1][j + 1] is the edit distane of first |i| characters of
  // |first_string_| and first |j| characters of |second_string_|
  int dp[len_first + 1][len_second + 1];

  // Initialize memory
  for (int i = 0; i < len_first + 1; i++) {
    dp[i][0] = i;
  }
  for (int j = 0; j < len_second + 1; j++) {
    dp[0][j] = j;
  }

  // Calculate the edit distance
  for (int i = 1; i < len_first + 1; i++) {
    for (int j = 1; j < len_second + 1; j++) {
      const int cost = first_string_[i - 1] == second_string_[j - 1] ? 0 : 1;
      // Insertion and deletion
      dp[i][j] = std::min(dp[i - 1][j], dp[i][j - 1]) + 1;
      // Substitution
      dp[i][j] = std::min(dp[i][j], dp[i - 1][j - 1] + cost);
      // Transposition
      if (i > 1 && j > 1 && first_string_[i - 2] == second_string_[j - 1] &&
          first_string_[i - 1] == second_string_[j - 2]) {
        dp[i][j] = std::min(dp[i][j], dp[i - 2][j - 2] + cost);
      }
    }
  }
  edit_distance_ = dp[len_first][len_second];
  return edit_distance_;
}

double SequenceMatcher::Ratio(bool use_edit_distance) {
  if (use_edit_distance) {
    if (edit_distance_ratio_ < 0) {
      const int edit_distance = EditDistance();
      edit_distance_ratio_ =
          1.0 - static_cast<double>(edit_distance) /
                    (first_string_.size() + second_string_.size());
    }
    return edit_distance_ratio_;
  }
  // TODO(crbug.com/990684): implement logic for the ratio calculation using
  // block matching.
  return block_matching_ratio_;
}

}  // namespace app_list
