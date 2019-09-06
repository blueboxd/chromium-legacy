// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_UTILS_FUZZY_TOKENIZED_STRING_MATCH_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_UTILS_FUZZY_TOKENIZED_STRING_MATCH_H_

#include "ash/public/cpp/app_list/tokenized_string.h"
#include "base/macros.h"
#include "ui/gfx/range/range.h"

namespace app_list {

class TokenizedString;

// FuzzyTokenizedStringMatch takes two tokenized strings: one as the text and
// the other one as the query. It matches the query against the text,
// calculates a relevance score between [0, 1] and marks the matched portions
// of text. A relevance of zero means the two are completely different to each
// other. The higher the relevance score, the better the two strings are
// matched. Matched portions of text are stored as index ranges.
class FuzzyTokenizedStringMatch {
 public:
  typedef std::vector<gfx::Range> Hits;

  FuzzyTokenizedStringMatch();
  ~FuzzyTokenizedStringMatch();

  // Calculates the relevance of two strings. Returns true if two strings are
  // somewhat matched, i.e. relevance score is greater than a threshold.
  bool IsRelevant(const TokenizedString& query, const TokenizedString& text);
  double relevance() const { return relevance_; }

 private:
  // Score in range of [0,1] representing how well the query matches the text.
  double relevance_ = 0;
  Hits hits_;

  DISALLOW_COPY_AND_ASSIGN(FuzzyTokenizedStringMatch);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_UTILS_FUZZY_TOKENIZED_STRING_MATCH_H_
