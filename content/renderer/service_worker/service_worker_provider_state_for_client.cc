// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_provider_state_for_client.h"

#include <utility>

#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace content {

ServiceWorkerProviderStateForClient::ServiceWorkerProviderStateForClient(
    scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory)
    : fallback_loader_factory(std::move(fallback_loader_factory)) {}

ServiceWorkerProviderStateForClient::~ServiceWorkerProviderStateForClient() {
  if (weak_wrapped_subresource_loader_factory) {
    weak_wrapped_subresource_loader_factory->Detach();
  }
}

}  // namespace content
