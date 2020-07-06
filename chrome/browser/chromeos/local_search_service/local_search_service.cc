// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/local_search_service.h"

#include <utility>

#include "chrome/browser/chromeos/local_search_service/linear_map_search.h"

namespace local_search_service {

LocalSearchService::LocalSearchService() = default;

LocalSearchService::~LocalSearchService() = default;

Index* LocalSearchService::GetIndex(IndexId index_id, Backend backend) {
  auto it = indices_.find(index_id);
  if (it == indices_.end()) {
    // TODO(jiameng): allow inverted index in the next cl.
    DCHECK_EQ(backend, Backend::kLinearMap);
    switch (backend) {
      case Backend::kLinearMap:
        it = indices_
                 .emplace(index_id, std::make_unique<LinearMapSearch>(index_id))
                 .first;
        break;
      default:
        it = indices_
                 .emplace(index_id, std::make_unique<LinearMapSearch>(index_id))
                 .first;
    }
  }
  DCHECK(it != indices_.end());
  DCHECK(it->second);

  return it->second.get();
}

}  // namespace local_search_service
