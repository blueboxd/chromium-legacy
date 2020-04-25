// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

namespace local_search_service {

class Index;

enum class IndexId { kCrosSettings = 0 };

// LocalSearchService creates and owns content-specific Indices. Clients can
// call it |GetIndex| method to get an Index for a given index id.
class LocalSearchService : public KeyedService {
 public:
  LocalSearchService();
  ~LocalSearchService() override;
  LocalSearchService(const LocalSearchService&) = delete;
  LocalSearchService& operator=(const LocalSearchService&) = delete;

  Index* GetIndex(local_search_service::IndexId index_id);

 private:
  std::map<local_search_service::IndexId, std::unique_ptr<Index>> indices_;
};

}  // namespace local_search_service

#endif  // CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_H_
