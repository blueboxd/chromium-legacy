// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SERVER_FAKE_SERVER_NETWORK_RESOURCES_H_
#define COMPONENTS_SYNC_TEST_FAKE_SERVER_FAKE_SERVER_NETWORK_RESOURCES_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/sync/engine/net/network_resources.h"
#include "components/sync/engine/net/network_time_update_callback.h"

namespace network {
class SharedURLLoaderFactoryInfo;
}  // namespace network

namespace fake_server {

class FakeServer;
class HttpPostProviderFactory;

class FakeServerNetworkResources : public syncer::NetworkResources {
 public:
  explicit FakeServerNetworkResources(
      const base::WeakPtr<FakeServer>& fake_server);
  ~FakeServerNetworkResources() override;

  // NetworkResources
  std::unique_ptr<syncer::HttpPostProviderFactory> GetHttpPostProviderFactory(
      const std::string& user_agent,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          url_loader_factory_info,
      const syncer::NetworkTimeUpdateCallback& network_time_update_callback)
      override;

 private:
  base::WeakPtr<FakeServer> fake_server_;
  scoped_refptr<base::SingleThreadTaskRunner> fake_server_task_runner_;
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_FAKE_SERVER_FAKE_SERVER_NETWORK_RESOURCES_H_
