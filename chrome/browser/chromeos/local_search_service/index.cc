// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/index.h"

#include <utility>

#include "base/optional.h"
#include "base/strings/string_util.h"
#include "chrome/common/string_matching/fuzzy_tokenized_string_match.h"
#include "chrome/common/string_matching/tokenized_string.h"

namespace local_search_service {

namespace {

using Hits = std::vector<local_search_service::Range>;

void TokenizeSearchTags(
    const std::vector<base::string16>& search_tags,
    std::vector<std::unique_ptr<TokenizedString>>* tokenized) {
  DCHECK(tokenized);
  for (const auto& tag : search_tags) {
    tokenized->push_back(std::make_unique<TokenizedString>(tag));
  }
}

// Returns whether a given item with |search_tags| is relevant to |query| using
// fuzzy string matching.
// TODO(1018613): add weight decay to relevance scores for search tags. Tags
// at the front should have higher scores.
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
    data_[id] = std::vector<std::unique_ptr<TokenizedString>>();
    TokenizeSearchTags(item.search_tags, &data_[id]);
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

void Index::GetSearchParamsForTesting(double* relevance_threshold,
                                      double* partial_match_penalty_rate,
                                      bool* use_prefix_only,
                                      bool* use_weighted_ratio,
                                      bool* use_edit_distance) {
  DCHECK(relevance_threshold);
  DCHECK(partial_match_penalty_rate);
  DCHECK(use_prefix_only);
  DCHECK(use_weighted_ratio);
  DCHECK(use_edit_distance);

  *relevance_threshold = search_params_.relevance_threshold;
  *partial_match_penalty_rate = search_params_.partial_match_penalty_rate;
  *use_prefix_only = search_params_.use_prefix_only;
  *use_weighted_ratio = search_params_.use_weighted_ratio;
  *use_edit_distance = search_params_.use_edit_distance;
}

std::vector<local_search_service::Result> Index::GetSearchResults(
    const base::string16& query,
    uint32_t max_results) const {
  std::vector<local_search_service::Result> results;
  const TokenizedString tokenized_query(query);

  for (const auto& item : data_) {
    double relevance_score = 0.0;
    Hits hits;
    if (IsItemRelevant(
            tokenized_query, item.second, search_params_.relevance_threshold,
            search_params_.use_prefix_only, search_params_.use_weighted_ratio,
            search_params_.use_edit_distance,
            search_params_.partial_match_penalty_rate, &relevance_score,
            &hits)) {
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
