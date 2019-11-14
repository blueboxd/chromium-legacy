// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/net/http_bridge_network_resources.h"

#include <utility>

#include "components/sync/engine/net/http_bridge.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace syncer {

HttpBridgeNetworkResources::~HttpBridgeNetworkResources() {}

std::unique_ptr<HttpPostProviderFactory>
HttpBridgeNetworkResources::GetHttpPostProviderFactory(
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info,
    const NetworkTimeUpdateCallback& network_time_update_callback) {
  return std::make_unique<HttpBridgeFactory>(std::move(url_loader_factory_info),
                                             network_time_update_callback);
}

}  // namespace syncer
