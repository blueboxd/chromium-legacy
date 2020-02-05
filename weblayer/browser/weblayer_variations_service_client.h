// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBLAYER_VARIATIONS_SERVICE_CLIENT_H_
#define WEBLAYER_BROWSER_WEBLAYER_VARIATIONS_SERVICE_CLIENT_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "components/variations/service/variations_service_client.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace weblayer {

// WebLayerVariationsServiceClient provides an implementation of
// VariationsServiceClient, all members are currently stubs for WebLayer.
class WebLayerVariationsServiceClient
    : public variations::VariationsServiceClient {
 public:
  explicit WebLayerVariationsServiceClient(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);
  ~WebLayerVariationsServiceClient() override;

 private:
  base::OnceCallback<base::Version(void)> GetVersionForSimulationCallback()
      override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;
  version_info::Channel GetChannel() override;
  bool OverridesRestrictParameter(std::string* parameter) override;
  bool IsEnterprise() override;

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebLayerVariationsServiceClient);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBLAYER_VARIATIONS_SERVICE_CLIENT_H_
