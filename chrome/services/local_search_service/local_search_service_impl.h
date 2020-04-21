// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_IMPL_H_
#define CHROME_SERVICES_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_IMPL_H_

#include <map>
#include <memory>

#include "base/macros.h"

namespace local_search_service {

class IndexImpl;

enum class IndexId { kCrosSettings = 0 };

// LocalSearchServiceImpl creates and owns content-specific Indices. Clients can
// call it |GetIndexImpl| method to get an Index for a given index id.
class LocalSearchServiceImpl {
 public:
  LocalSearchServiceImpl();
  ~LocalSearchServiceImpl();
  LocalSearchServiceImpl(const LocalSearchServiceImpl&) = delete;
  LocalSearchServiceImpl& operator=(const LocalSearchServiceImpl&) = delete;

  IndexImpl* GetIndexImpl(local_search_service::IndexId index_id);

 private:
  std::map<local_search_service::IndexId, std::unique_ptr<IndexImpl>> indices_;
};

}  // namespace local_search_service

#endif  // CHROME_SERVICES_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_IMPL_H_
