// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_NETWORK_CONTEXT_H_
#define CHROME_BROWSER_CHROMEOS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_NETWORK_CONTEXT_H_

#include "base/macros.h"
#include "chrome/browser/net/proxy_config_monitor.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace chromeos {

class WilcoDtcSupportdNetworkContext {
 public:
  virtual ~WilcoDtcSupportdNetworkContext() = default;

  virtual network::mojom::URLLoaderFactory* GetURLLoaderFactory() = 0;
};

class WilcoDtcSupportdNetworkContextImpl
    : public WilcoDtcSupportdNetworkContext {
 public:
  WilcoDtcSupportdNetworkContextImpl();
  ~WilcoDtcSupportdNetworkContextImpl() override;

  // WilcoDtcSupportdNetworkContext overrides:
  network::mojom::URLLoaderFactory* GetURLLoaderFactory() override;

  void FlushForTesting();

 private:
  // Ensures that Network Context created and connected to the network service.
  void EnsureNetworkContextExists();

  // Creates Network Context.
  void CreateNetworkContext();

  ProxyConfigMonitor proxy_config_monitor_;

  // NetworkContext using the network service.
  mojo::Remote<network::mojom::NetworkContext> network_context_;

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(WilcoDtcSupportdNetworkContextImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_NETWORK_CONTEXT_H_
