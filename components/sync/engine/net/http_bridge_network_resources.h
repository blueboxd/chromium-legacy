// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NET_HTTP_BRIDGE_NETWORK_RESOURCES_H_
#define COMPONENTS_SYNC_ENGINE_NET_HTTP_BRIDGE_NETWORK_RESOURCES_H_

#include <memory>

#include "components/sync/engine/net/network_resources.h"
#include "components/sync/engine/net/network_time_update_callback.h"

namespace network {
class SharedURLLoaderFactoryInfo;
}  // namespace network

namespace syncer {

class HttpPostProviderFactory;

class HttpBridgeNetworkResources : public NetworkResources {
 public:
  ~HttpBridgeNetworkResources() override;

  // NetworkResources
  std::unique_ptr<HttpPostProviderFactory> GetHttpPostProviderFactory(
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          url_loader_factory_info,
      const NetworkTimeUpdateCallback& network_time_update_callback) override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_NET_HTTP_BRIDGE_NETWORK_RESOURCES_H_
