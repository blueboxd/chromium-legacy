// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_PROXY_H_
#define CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_PROXY_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace local_search_service {

class LocalSearchService;

// TODO(jiameng): the next cl will remove this class completely because the
// factory will return LocalSearchService (that will be a KeyedService).
class LocalSearchServiceProxy : public KeyedService {
 public:
  // Profile isn't required, hence can be nullptr in tests.
  explicit LocalSearchServiceProxy(Profile* profile);
  ~LocalSearchServiceProxy() override;

  LocalSearchServiceProxy(const LocalSearchServiceProxy&) = delete;
  LocalSearchServiceProxy& operator=(const LocalSearchServiceProxy&) = delete;

  LocalSearchService* GetLocalSearchService();

 private:
  std::unique_ptr<LocalSearchService> local_search_service_;

  base::WeakPtrFactory<LocalSearchServiceProxy> weak_ptr_factory_{this};
};

}  // namespace local_search_service

#endif  // CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_PROXY_H_
