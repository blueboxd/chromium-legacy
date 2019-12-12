// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/local_search_service/index_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/string_matching/fuzzy_tokenized_string_match.h"
#include "chrome/common/string_matching/tokenized_string.h"

namespace local_search_service {

namespace {

using Hits = std::vector<mojom::RangePtr>;

void TokenizeSearchTags(
    const std::vector<std::string>& search_tags,
    std::vector<std::unique_ptr<TokenizedString>>* tokenized) {
  DCHECK(tokenized);
  for (const auto& tag : search_tags) {
    tokenized->push_back(
        std::make_unique<TokenizedString>(base::UTF8ToUTF16(tag)));
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
        mojom::RangePtr range = mojom::Range::New(hit.start(), hit.end());
        hits->push_back(std::move(range));
      }
      return true;
    }
  }
  return false;
}

// Compares mojom::ResultPtr by |score|.
bool CompareResultPtr(const mojom::ResultPtr& r1, const mojom::ResultPtr& r2) {
  return r1->score > r2->score;
}

}  // namespace

IndexImpl::IndexImpl() = default;

IndexImpl::~IndexImpl() = default;

void IndexImpl::BindReceiver(mojo::PendingReceiver<mojom::Index> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void IndexImpl::GetSize(GetSizeCallback callback) {
  std::move(callback).Run(data_.size());
}

void IndexImpl::AddOrUpdate(std::vector<mojom::DataPtr> data,
                            AddOrUpdateCallback callback) {
  for (const auto& item : data) {
    const auto& id = item->id;
    // Keys shouldn't be empty.
    if (id.empty())
      receivers_.ReportBadMessage("Empty ID in updated data");

    // If a key already exists, it will overwrite earlier data.
    data_[id] = std::vector<std::unique_ptr<TokenizedString>>();
    TokenizeSearchTags(item->search_tags, &data_[id]);
  }
  std::move(callback).Run();
}

void IndexImpl::Delete(const std::vector<std::string>& ids,
                       DeleteCallback callback) {
  uint32_t num_deleted = 0u;
  for (const auto& id : ids) {
    // Keys shouldn't be empty.
    if (id.empty())
      receivers_.ReportBadMessage("Empty ID in deleted data");

    const auto& it = data_.find(id);
    if (it != data_.end()) {
      // If id doesn't exist, just ignore it.
      data_.erase(id);
      ++num_deleted;
    }
  }
  std::move(callback).Run(num_deleted);
}

void IndexImpl::Find(const std::string& query,
                     int32_t max_latency_in_ms,
                     int32_t max_results,
                     FindCallback callback) {
  if (query.empty()) {
    std::move(callback).Run(mojom::ResponseStatus::EMPTY_QUERY, base::nullopt);
    return;
  }
  if (data_.empty()) {
    std::move(callback).Run(mojom::ResponseStatus::EMPTY_INDEX, base::nullopt);
    return;
  }

  std::vector<mojom::ResultPtr> results = GetSearchResults(query);
  std::move(callback).Run(mojom::ResponseStatus::SUCCESS, std::move(results));
}

void IndexImpl::SetSearchParams(mojom::SearchParamsPtr search_params,
                                SetSearchParamsCallback callback) {
  search_params_ = std::move(search_params);
  std::move(callback).Run();
}

void IndexImpl::GetSearchParamsForTesting(double* relevance_threshold,
                                          double* partial_match_penalty_rate,
                                          bool* use_prefix_only,
                                          bool* use_weighted_ratio,
                                          bool* use_edit_distance) {
  DCHECK(relevance_threshold);
  DCHECK(partial_match_penalty_rate);
  DCHECK(use_prefix_only);
  DCHECK(use_weighted_ratio);
  DCHECK(use_edit_distance);

  *relevance_threshold = search_params_->relevance_threshold;
  *partial_match_penalty_rate = search_params_->partial_match_penalty_rate;
  *use_prefix_only = search_params_->use_prefix_only;
  *use_weighted_ratio = search_params_->use_weighted_ratio;
  *use_edit_distance = search_params_->use_edit_distance;
}

std::vector<mojom::ResultPtr> IndexImpl::GetSearchResults(
    const std::string& query) const {
  std::vector<mojom::ResultPtr> results;
  const TokenizedString tokenized_query(base::UTF8ToUTF16(query));

  for (const auto& item : data_) {
    double relevance_score = 0.0;
    Hits hits;
    if (IsItemRelevant(
            tokenized_query, item.second, search_params_->relevance_threshold,
            search_params_->use_prefix_only, search_params_->use_weighted_ratio,
            search_params_->use_edit_distance,
            search_params_->partial_match_penalty_rate, &relevance_score,
            &hits)) {
      mojom::ResultPtr result = mojom::Result::New();
      result->id = item.first;
      result->score = relevance_score;
      result->hits = std::move(hits);
      results.push_back(std::move(result));
    }
  }

  std::sort(results.begin(), results.end(), CompareResultPtr);
  return results;
}

}  // namespace local_search_service
