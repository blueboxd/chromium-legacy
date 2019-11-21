// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/local_search_service/index_impl.h"

#include <utility>

#include "base/bind.h"
#include "chrome/services/local_search_service/public/mojom/types.mojom.h"

namespace local_search_service {

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
    data_[id] = item->search_tags;
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

// TODO(crbug.com/1018613): only returning the exact match for now and only
// search based on the id of the data.
std::vector<mojom::ResultPtr> IndexImpl::GetSearchResults(
    const std::string& query) const {
  std::vector<mojom::ResultPtr> results;
  for (const auto& item : data_) {
    if (item.first == query) {
      mojom::ResultPtr result = mojom::Result::New(item.first, /*score=*/1.0);
      results.push_back(std::move(result));
      return results;
    }
  }
  return results;
}

}  // namespace local_search_service
