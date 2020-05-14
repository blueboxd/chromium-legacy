// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/index.h"

#include <utility>

#include "base/optional.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/common/string_matching/fuzzy_tokenized_string_match.h"
#include "chrome/common/string_matching/tokenized_string.h"

namespace local_search_service {

namespace {

using Hits = std::vector<local_search_service::Range>;

// |individual_tokenized| can be nullptr if there is no need to split
// the input search tags by space.
void TokenizeSearchTags(
    const std::vector<base::string16>& search_tags,
    std::vector<std::unique_ptr<TokenizedString>>* tokenized,
    std::vector<std::unique_ptr<TokenizedString>>* individual_tokenized) {
  DCHECK(tokenized);

  bool has_multiple_words = false;
  std::set<base::string16> unique_tags;
  for (const auto& tag : search_tags) {
    tokenized->push_back(std::make_unique<TokenizedString>(tag));
    if (individual_tokenized) {
      const std::vector<base::string16> words =
          base::SplitString(tag, base::kWhitespaceASCIIAs16,
                            base::WhitespaceHandling::TRIM_WHITESPACE,
                            base::SplitResult::SPLIT_WANT_NONEMPTY);
      std::copy(words.begin(), words.end(),
                std::inserter(unique_tags, unique_tags.end()));
      if (words.size() > 1) {
        has_multiple_words = true;
      }
    }
  }

  if (!has_multiple_words)
    return;

  DCHECK(individual_tokenized);
  for (const auto& tag : unique_tags) {
    individual_tokenized->push_back(std::make_unique<TokenizedString>(tag));
  }
}

// Returns whether a given item with |search_tags| is relevant to |query| using
// fuzzy string matching.
bool IsItemRelevant(
    const TokenizedString& query,
    const std::vector<std::unique_ptr<TokenizedString>>& search_tags,
    double relevance_threshold,
    bool use_prefix_only,
    bool use_weighted_ratio,
    bool use_edit_distance,
    double partial_match_penalty_rate,
    double* relevance_score,
    Hits* hits) {
  DCHECK(relevance_score);
  DCHECK(hits);

  if (search_tags.empty())
    return false;

  for (const auto& tag : search_tags) {
    FuzzyTokenizedStringMatch match;
    if (match.IsRelevant(query, *tag, relevance_threshold, use_prefix_only,
                         use_weighted_ratio, use_edit_distance,
                         partial_match_penalty_rate)) {
      *relevance_score = match.relevance();
      for (const auto& hit : match.hits()) {
        local_search_service::Range range;
        range.start = hit.start();
        range.end = hit.end();
        hits->push_back(range);
      }
      return true;
    }
  }
  return false;
}

// Compares Results by |score|.
bool CompareResults(const local_search_service::Result& r1,
                    const local_search_service::Result& r2) {
  return r1.score > r2.score;
}

}  // namespace

local_search_service::Data::Data(const std::string& id,
                                 const std::vector<base::string16>& search_tags)
    : id(id), search_tags(search_tags) {}
local_search_service::Data::Data() = default;
local_search_service::Data::Data(const Data& data) = default;
local_search_service::Data::~Data() = default;
local_search_service::Result::Result() = default;
local_search_service::Result::Result(const Result& result) = default;
local_search_service::Result::~Result() = default;

Index::Index() = default;

Index::~Index() = default;

uint64_t Index::GetSize() {
  return data_.size();
}

void Index::AddOrUpdate(const std::vector<local_search_service::Data>& data) {
  for (const auto& item : data) {
    const auto& id = item.id;
    DCHECK(!id.empty());

    // If a key already exists, it will overwrite earlier data.
    data_[id] = {std::vector<std::unique_ptr<TokenizedString>>(),
                 std::vector<std::unique_ptr<TokenizedString>>()};
    auto& tokenized = data_[id];
    TokenizeSearchTags(
        item.search_tags, &tokenized.first,
        search_params_.split_search_tags ? &tokenized.second : nullptr);
  }
}

uint32_t Index::Delete(const std::vector<std::string>& ids) {
  uint32_t num_deleted = 0u;
  for (const auto& id : ids) {
    DCHECK(!id.empty());

    const auto& it = data_.find(id);
    if (it != data_.end()) {
      // If id doesn't exist, just ignore it.
      data_.erase(id);
      ++num_deleted;
    }
  }
  return num_deleted;
}

local_search_service::ResponseStatus Index::Find(
    const base::string16& query,
    uint32_t max_results,
    std::vector<local_search_service::Result>* results) {
  DCHECK(results);
  results->clear();
  if (query.empty()) {
    return local_search_service::ResponseStatus::kEmptyQuery;
  }
  if (data_.empty()) {
    return local_search_service::ResponseStatus::kEmptyIndex;
  }

  *results = GetSearchResults(query, max_results);
  return local_search_service::ResponseStatus::kSuccess;
}

void Index::SetSearchParams(
    const local_search_service::SearchParams& search_params) {
  search_params_ = search_params;
}

void Index::GetSearchTagsForTesting(
    const std::string& id,
    std::vector<base::string16>* search_tags,
    std::vector<base::string16>* individual_search_tags) {
  DCHECK(search_tags);
  DCHECK(individual_search_tags);

  search_tags->clear();
  individual_search_tags->clear();

  const auto& it = data_.find(id);
  if (it != data_.end()) {
    for (const auto& tag : it->second.first) {
      search_tags->push_back(tag->text());
    }
    for (const auto& tag : it->second.second) {
      individual_search_tags->push_back(tag->text());
    }
  }
}

SearchParams Index::GetSearchParamsForTesting() {
  return search_params_;
}

// For each data item, each of its search tag could be a single word
// or multiple words. When we match a query with a search tag, we could match
// the query with the full search tag, or with individual words in the search
// tag.
// 1. If the query itself is a single word, then we consider it a match if the
// query matches with a word of the search tag. In this case, we use simple
// fuzzy ratio and prefix matching (instead of weighted ratio) for better
// accuracy and speeds. However, if the search tag is not split into words, then
// we would need weighted ratio to discover the match. The accuracy may be
// lower.
// 2. If the query contains multiple words, then we will have to match it with
// the full search tag because we do not split the query words.
// TODO(jiameng): this is complex and multi-word query and search tags should
// really be handled by TF-IDF based matching. We will soon move to TF-IDF
// method.
std::vector<local_search_service::Result> Index::GetSearchResults(
    const base::string16& query,
    uint32_t max_results) const {
  const std::vector<base::string16> query_words =
      base::SplitString(query, base::kWhitespaceASCIIAs16,
                        base::WhitespaceHandling::TRIM_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_NONEMPTY);
  const bool query_has_multiple_words = query_words.size() > 1;
  const bool use_weighted_ratio =
      query_has_multiple_words || !search_params_.split_search_tags;

  std::vector<local_search_service::Result> results;
  const TokenizedString tokenized_query(query);

  for (const auto& item : data_) {
    double relevance_score = 0.0;
    Hits hits;
    // Use the full search tags if we use weighted ratio or if this data item
    // has not split search tags.
    const auto& used_search_tags =
        (use_weighted_ratio || item.second.second.empty()) ? item.second.first
                                                           : item.second.second;
    if (IsItemRelevant(tokenized_query, used_search_tags,
                       search_params_.relevance_threshold,
                       search_params_.use_prefix_only, use_weighted_ratio,
                       search_params_.use_edit_distance,
                       search_params_.partial_match_penalty_rate,
                       &relevance_score, &hits)) {
      local_search_service::Result result;
      result.id = item.first;
      result.score = relevance_score;
      result.hits = hits;
      results.push_back(result);
    }
  }

  std::sort(results.begin(), results.end(), CompareResults);
  if (results.size() > max_results && max_results > 0u) {
    results.resize(max_results);
  }
  return results;
}

}  // namespace local_search_service
