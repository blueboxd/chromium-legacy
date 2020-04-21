// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/local_search_service/local_search_service_impl.h"

#include <utility>

#include "chrome/services/local_search_service/index_impl.h"

namespace local_search_service {

LocalSearchServiceImpl::LocalSearchServiceImpl() = default;

LocalSearchServiceImpl::~LocalSearchServiceImpl() = default;

IndexImpl* LocalSearchServiceImpl::GetIndexImpl(
    local_search_service::IndexId index_id) {
  auto it = indices_.find(index_id);
  if (it == indices_.end())
    it = indices_.emplace(index_id, std::make_unique<IndexImpl>()).first;

  DCHECK(it != indices_.end());
  DCHECK(it->second);

  return it->second.get();
}

}  // namespace local_search_service
