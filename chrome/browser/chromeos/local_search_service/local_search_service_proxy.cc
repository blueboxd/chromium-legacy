// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/local_search_service_proxy.h"

#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/local_search_service/local_search_service.h"

namespace local_search_service {

LocalSearchServiceProxy::LocalSearchServiceProxy(Profile* profile) {}

LocalSearchServiceProxy::~LocalSearchServiceProxy() = default;

LocalSearchService* LocalSearchServiceProxy::GetLocalSearchService() {
  if (!local_search_service_) {
    local_search_service_ = std::make_unique<LocalSearchService>();
  }
  return local_search_service_.get();
}

}  // namespace local_search_service
