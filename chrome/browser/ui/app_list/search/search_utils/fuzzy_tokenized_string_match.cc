// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_utils/fuzzy_tokenized_string_match.h"

#include <algorithm>
#include <iterator>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/search/search_utils/sequence_matcher.h"

namespace app_list {

namespace {
const double kRelevanceThreshold = 0.6;

// Returns sorted tokens from a TokenizedString.
std::vector<base::string16> ProcessAndSort(const TokenizedString& text) {
  std::vector<base::string16> result;
  for (const auto& token : text.tokens()) {
    result.emplace_back(token);
  }
  std::sort(result.begin(), result.end());
  return result;
}
}  // namespace

FuzzyTokenizedStringMatch::~FuzzyTokenizedStringMatch() {}
FuzzyTokenizedStringMatch::FuzzyTokenizedStringMatch() {}

double FuzzyTokenizedStringMatch::TokenSetRatio(const TokenizedString& query,
                                                const TokenizedString& text,
                                                bool partial) {
  std::set<base::string16> query_token(query.tokens().begin(),
                                       query.tokens().end());
  std::set<base::string16> text_token(text.tokens().begin(),
                                      text.tokens().end());

  std::vector<base::string16> intersection;
  std::vector<base::string16> query_diff_text;
  std::vector<base::string16> text_diff_query;

  // Find the intersection and the differences between two set of tokens.
  std::set_intersection(query_token.begin(), query_token.end(),
                        text_token.begin(), text_token.end(),
                        std::back_inserter(intersection));
  std::set_difference(query_token.begin(), query_token.end(),
                      text_token.begin(), text_token.end(),
                      std::back_inserter(query_diff_text));
  std::set_difference(text_token.begin(), text_token.end(), query_token.begin(),
                      query_token.end(), std::back_inserter(text_diff_query));

  const base::string16 intersection_string =
      base::JoinString(intersection, base::UTF8ToUTF16(" "));
  const base::string16 query_rewritten =
      intersection.empty()
          ? base::JoinString(query_diff_text, base::UTF8ToUTF16(" "))
          : base::StrCat(
                {intersection_string, base::UTF8ToUTF16(" "),
                 base::JoinString(query_diff_text, base::UTF8ToUTF16(" "))});
  const base::string16 text_rewritten =
      intersection.empty()
          ? base::JoinString(text_diff_query, base::UTF8ToUTF16(" "))
          : base::StrCat(
                {intersection_string, base::UTF8ToUTF16(" "),
                 base::JoinString(text_diff_query, base::UTF8ToUTF16(" "))});

  if (partial) {
    return std::max({PartialRatio(intersection_string, query_rewritten),
                     PartialRatio(intersection_string, text_rewritten),
                     PartialRatio(query_rewritten, text_rewritten)});
  }

  return std::max({SequenceMatcher(intersection_string, query_rewritten)
                       .Ratio(false /*use_edit_distance*/),
                   SequenceMatcher(intersection_string, text_rewritten)
                       .Ratio(false /*use_edit_distance*/),
                   SequenceMatcher(query_rewritten, text_rewritten)
                       .Ratio(false /*use_edit_distance*/)});
}

double FuzzyTokenizedStringMatch::TokenSortRatio(const TokenizedString& query,
                                                 const TokenizedString& text,
                                                 bool partial) {
  const base::string16 query_sorted =
      base::JoinString(ProcessAndSort(query), base::UTF8ToUTF16(" "));
  const base::string16 text_sorted =
      base::JoinString(ProcessAndSort(text), base::UTF8ToUTF16(" "));

  if (partial) {
    return PartialRatio(query_sorted, text_sorted);
  }
  return SequenceMatcher(query_sorted, text_sorted)
      .Ratio(false /*use_edit_distance*/);
}

double FuzzyTokenizedStringMatch::PartialRatio(const base::string16& query,
                                               const base::string16& text) {
  if (query.empty() || text.empty()) {
    return 0.0;
  }
  base::string16 shorter = query;
  base::string16 longer = text;

  if (shorter.size() > longer.size()) {
    shorter = text;
    longer = query;
  }

  const auto matching_blocks =
      SequenceMatcher(shorter, longer).GetMatchingBlocks();
  double partial_ratio = 0;

  for (const auto& block : matching_blocks) {
    const int long_start =
        block.pos_second_string > block.pos_first_string
            ? block.pos_second_string - block.pos_first_string
            : 0;

    // TODO(crbug/990684): currently this part re-calculate the ratio for every
    // pair. Improve this to reduce latency.
    partial_ratio = std::max(
        partial_ratio,
        SequenceMatcher(shorter, longer.substr(long_start, shorter.size()))
            .Ratio(false /*use_edit_distance*/));
    if (partial_ratio > 0.995) {
      return 1;
    }
  }
  return partial_ratio;
}

double FuzzyTokenizedStringMatch::WeightedRatio(const TokenizedString& query,
                                                const TokenizedString& text) {
  const double unbase_scale = 0.95;
  double weighted_ratio = SequenceMatcher(query.text(), text.text())
                              .Ratio(false /*use_edit_distance*/);
  const double length_ratio =
      static_cast<double>(std::max(query.text().size(), text.text().size())) /
      std::min(query.text().size(), text.text().size());

  // Use partial if two strings are quite different in sizes.
  const bool use_partial = length_ratio >= 1.5;
  double partial_scale = 1;

  if (use_partial) {
    // If one string is much much shorter than the other, set |partial_scale| to
    // be 0.6, otherwise set it to be 0.9.
    partial_scale = length_ratio > 8 ? 0.6 : 0.9;
    weighted_ratio =
        std::max(weighted_ratio,
                 PartialRatio(query.text(), text.text()) * partial_scale);
  }
  weighted_ratio = std::max(
      weighted_ratio, TokenSortRatio(query, text, /*partial=*/use_partial) *
                          unbase_scale * partial_scale);
  weighted_ratio = std::max(
      weighted_ratio, TokenSetRatio(query, text, /*partial=*/use_partial) *
                          unbase_scale * partial_scale);
  return weighted_ratio;
}

bool FuzzyTokenizedStringMatch::IsRelevant(const TokenizedString& query,
                                           const TokenizedString& text) {
  // TODO(crbug.com/990684): add prefix matching logic.
  relevance_ = WeightedRatio(query, text);
  return relevance_ > kRelevanceThreshold;
}

}  // namespace app_list
