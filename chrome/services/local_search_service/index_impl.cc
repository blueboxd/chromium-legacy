// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/local_search_service/index_impl.h"

#include <utility>

#include "base/bind.h"
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

IndexImpl::IndexImpl() = default;

IndexImpl::~IndexImpl() = default;

void IndexImpl::BindReceiver(mojo::PendingReceiver<mojom::Index> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void IndexImpl::GetSize(GetSizeCallback callback) {
  const uint64_t size = GetSize();
  std::move(callback).Run(size);
}

uint64_t IndexImpl::GetSize() {
  return data_.size();
}

void IndexImpl::AddOrUpdate(std::vector<mojom::DataPtr> data,
                            AddOrUpdateCallback callback) {
  std::vector<local_search_service::Data> data_in;
  for (const auto& d : data) {
    if (d->id.empty())
      receivers_.ReportBadMessage("Empty ID in updated data");

    local_search_service::Data d_in;
    d_in.id = d->id;
    d_in.search_tags = d->search_tags;
    data_in.push_back(d_in);
  }

  AddOrUpdate(data_in);
  std::move(callback).Run();
}

void IndexImpl::AddOrUpdate(
    const std::vector<local_search_service::Data>& data) {
  for (const auto& item : data) {
    const auto& id = item.id;
    DCHECK(!id.empty());

    // If a key already exists, it will overwrite earlier data.
    data_[id] = std::vector<std::unique_ptr<TokenizedString>>();
    TokenizeSearchTags(item.search_tags, &data_[id]);
  }
}

void IndexImpl::Delete(const std::vector<std::string>& ids,
                       DeleteCallback callback) {
  for (const auto& id : ids) {
    if (id.empty())
      receivers_.ReportBadMessage("Empty ID in deleted data");
  }
  const uint32_t num_deleted = Delete(ids);
  std::move(callback).Run(num_deleted);
}

uint32_t IndexImpl::Delete(const std::vector<std::string>& ids) {
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

void IndexImpl::Find(const base::string16& query,
                     int32_t max_latency_in_ms,
                     int32_t max_results,
                     FindCallback callback) {
  std::vector<local_search_service::Result> results;
  // TODO(jiameng): |max_latency| isn't supported yet. We're
  // temporarily ignoring it before the next cl removes the async call.
  const auto response =
      Find(query, max_results < 0 ? 0u : max_results, &results);

  mojom::ResponseStatus mresponse = mojom::ResponseStatus::UNKNOWN_ERROR;
  switch (response) {
    case local_search_service::ResponseStatus::kEmptyQuery:
      mresponse = mojom::ResponseStatus::EMPTY_QUERY;
      break;
    case local_search_service::ResponseStatus::kEmptyIndex:
      mresponse = mojom::ResponseStatus::EMPTY_INDEX;
      break;
    case local_search_service::ResponseStatus::kSuccess:
      mresponse = mojom::ResponseStatus::SUCCESS;
      break;
    default:
      break;
  }

  if (mresponse != mojom::ResponseStatus::SUCCESS) {
    std::move(callback).Run(mresponse, base::nullopt);
    return;
  }

  std::vector<mojom::ResultPtr> mresults;
  for (const auto& r : results) {
    mojom::ResultPtr mr = mojom::Result::New();
    mr->id = r.id;
    mr->score = r.score;
    std::vector<mojom::RangePtr> mhits;
    for (const auto& hit : r.hits) {
      mojom::RangePtr range = mojom::Range::New(hit.start, hit.end);
      mhits.push_back(std::move(range));
    }
    mr->hits = std::move(mhits);
    mresults.push_back(std::move(mr));
  }

  std::move(callback).Run(mojom::ResponseStatus::SUCCESS, std::move(mresults));
}

local_search_service::ResponseStatus IndexImpl::Find(
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

void IndexImpl::SetSearchParams(mojom::SearchParamsPtr search_params,
                                SetSearchParamsCallback callback) {
  local_search_service::SearchParams search_params_in;
  search_params_in.relevance_threshold = search_params->relevance_threshold;
  search_params_in.partial_match_penalty_rate =
      search_params->partial_match_penalty_rate;
  search_params_in.use_prefix_only = search_params->use_prefix_only;
  search_params_in.use_weighted_ratio = search_params->use_weighted_ratio;
  search_params_in.use_edit_distance = search_params->use_edit_distance;
  SetSearchParams(search_params_in);
  std::move(callback).Run();
}

void IndexImpl::SetSearchParams(
    const local_search_service::SearchParams& search_params) {
  search_params_ = search_params;
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

  *relevance_threshold = search_params_.relevance_threshold;
  *partial_match_penalty_rate = search_params_.partial_match_penalty_rate;
  *use_prefix_only = search_params_.use_prefix_only;
  *use_weighted_ratio = search_params_.use_weighted_ratio;
  *use_edit_distance = search_params_.use_edit_distance;
}

std::vector<local_search_service::Result> IndexImpl::GetSearchResults(
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
