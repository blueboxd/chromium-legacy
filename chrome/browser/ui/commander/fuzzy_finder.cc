// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/fuzzy_finder.h"

#include "base/i18n/case_conversion.h"

namespace commander {

double FuzzyFind(const base::string16& needle,
                 const base::string16& haystack,
                 std::vector<gfx::Range>* matched_ranges) {
  DCHECK(needle == base::i18n::FoldCase(needle));
  matched_ranges->clear();
  const base::string16& folded = base::i18n::FoldCase(haystack);
  if (folded == needle) {
    matched_ranges->emplace_back(0, needle.length());
    return 1.0;
  }
  size_t substring_position = folded.find(needle);
  if (substring_position == std::string::npos)
    return 0;
  matched_ranges->emplace_back(substring_position, needle.length());
  if (substring_position == 0)
    return .99;
  return std::min(1 - static_cast<double>(substring_position) / folded.length(),
                  0.01);
}

}  // namespace commander
