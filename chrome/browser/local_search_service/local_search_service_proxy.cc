// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_search_service/local_search_service_proxy.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/services/local_search_service/local_search_service_impl.h"

namespace local_search_service {

LocalSearchServiceProxy::LocalSearchServiceProxy(Profile* profile) {}

LocalSearchServiceProxy::~LocalSearchServiceProxy() = default;

mojom::LocalSearchService* LocalSearchServiceProxy::GetLocalSearchService() {
  if (!local_search_service_impl_) {
    local_search_service_impl_ = std::make_unique<LocalSearchServiceImpl>();
    local_search_service_impl_->BindReceiver(
        remote_.BindNewPipeAndPassReceiver());
  }
  return remote_.get();
}

}  // namespace local_search_service
