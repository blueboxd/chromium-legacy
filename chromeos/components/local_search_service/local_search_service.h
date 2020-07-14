// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "chromeos/components/local_search_service/shared_structs.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace chromeos {

namespace local_search_service {

class Index;

// LocalSearchService creates and owns content-specific Indices. Clients can
// call it |GetIndex| method to get an Index for a given index id.
class LocalSearchService : public KeyedService {
 public:
  LocalSearchService();
  ~LocalSearchService() override;
  LocalSearchService(const LocalSearchService&) = delete;
  LocalSearchService& operator=(const LocalSearchService&) = delete;

  Index* GetIndex(IndexId index_id, Backend backend, PrefService* local_state);

 private:
  std::map<IndexId, std::unique_ptr<Index>> indices_;
};

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_H_
