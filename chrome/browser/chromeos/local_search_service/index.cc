// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/index.h"

#include <utility>

#include "chrome/browser/chromeos/local_search_service/linear_map_search.h"

namespace local_search_service {

Index::Index() {
  linear_map_search_ = std::make_unique<LinearMapSearch>();
}

Index::~Index() = default;

uint64_t Index::GetSize() {
  DCHECK(linear_map_search_);
  return linear_map_search_->GetSize();
}

void Index::AddOrUpdate(const std::vector<local_search_service::Data>& data) {
  DCHECK(linear_map_search_);
  linear_map_search_->AddOrUpdate(data);
}

uint32_t Index::Delete(const std::vector<std::string>& ids) {
  DCHECK(linear_map_search_);
  const uint32_t num_deleted = linear_map_search_->Delete(ids);
  return num_deleted;
}

ResponseStatus Index::Find(const base::string16& query,
                           uint32_t max_results,
                           std::vector<Result>* results) {
  DCHECK(linear_map_search_);
  return linear_map_search_->Find(query, max_results, results);
}

void Index::SetSearchParams(const SearchParams& search_params) {
  DCHECK(linear_map_search_);
  linear_map_search_->SetSearchParams(search_params);
}

SearchParams Index::GetSearchParamsForTesting() {
  DCHECK(linear_map_search_);
  return linear_map_search_->GetSearchParams();
}


}  // namespace local_search_service
