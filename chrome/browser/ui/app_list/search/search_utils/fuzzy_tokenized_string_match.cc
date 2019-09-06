// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_utils/fuzzy_tokenized_string_match.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/search/search_utils/sequence_matcher.h"

namespace app_list {

namespace {
const double kRelevanceThreshold = 0.6;

double PartialRatio(const TokenizedString& query,
                    const TokenizedString& text,
                    SequenceMatcher& sequence_matcher) {
  // TODO(crbug.com/990684): implement the logic of this function.
  return 0.0;
}

double PartialTokenSetRatio(const TokenizedString& query,
                            const TokenizedString& text,
                            SequenceMatcher& sequence_matcher) {
  // TODO(crbug.com/990684): implement the logic of this function.
  return 0.0;
}

double PartialTokenSortRatio(const TokenizedString& query,
                             const TokenizedString& text,
                             SequenceMatcher& sequence_matcher) {
  // TODO(crbug.com/990684): implement the logic of this function.
  return 0.0;
}

double TokenSortRatio(const TokenizedString& query,
                      const TokenizedString& text,
                      SequenceMatcher& sequence_matcher) {
  // TODO(crbug.com/990684): implement the logic of this function.
  return 0.0;
}

double TokenSetRatio(const TokenizedString& query,
                     const TokenizedString& text,
                     SequenceMatcher& sequence_matcher) {
  // TODO(crbug.com/990684): implement the logic of this function.
  return 0.0;
}

double WeightedRatio(const TokenizedString& query,
                     const TokenizedString& text,
                     SequenceMatcher& sequence_matcher) {
  const double unbase_scale = 0.95;
  double weighted_ratio = sequence_matcher.Ratio();
  const double length_ratio =
      static_cast<double>(std::max(query.text().size(), text.text().size())) /
      std::min(query.text().size(), text.text().size());

  // Use partial if two strings are quite different in sizes.
  if (length_ratio >= 1.5) {
    // If one string is much much shorter than the other, set |partial_scale| to
    // be 0.6, otherwise set it to be 0.9.
    const double partial_scale = length_ratio > 8 ? 0.6 : 0.9;
    weighted_ratio =
        std::max(weighted_ratio,
                 PartialRatio(query, text, sequence_matcher) * partial_scale);
    weighted_ratio = std::max(
        weighted_ratio, PartialTokenSortRatio(query, text, sequence_matcher) *
                            unbase_scale * partial_scale);
    weighted_ratio = std::max(
        weighted_ratio, PartialTokenSetRatio(query, text, sequence_matcher) *
                            unbase_scale * partial_scale);
    return weighted_ratio;
  }
  // If strings are similar length, don't use partial.
  weighted_ratio =
      std::max(weighted_ratio,
               TokenSortRatio(query, text, sequence_matcher) * unbase_scale);
  weighted_ratio =
      std::max(weighted_ratio,
               TokenSetRatio(query, text, sequence_matcher) * unbase_scale);
  return weighted_ratio;
}
}  // namespace

bool FuzzyTokenizedStringMatch::IsRelevant(const TokenizedString& query,
                                           const TokenizedString& text) {
  // TODO(crbug.com/990684): add prefix matching logic.
  SequenceMatcher sequence_matcher(base::UTF16ToUTF8(query.text()),
                                   base::UTF16ToUTF8(text.text()));
  relevance_ = WeightedRatio(query, text, sequence_matcher);
  return relevance_ > kRelevanceThreshold;
}

}  // namespace app_list
