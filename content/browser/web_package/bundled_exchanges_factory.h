// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_FACTORY_H_
#define CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "content/browser/web_package/bundled_exchanges_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace content {

class NavigationLoaderInterceptor;

// A class to provide interfaces to communicate with a BundledExchanges for
// loading.
class BundledExchangesFactory final {
 public:
  explicit BundledExchangesFactory(const BundledExchangesSource& source);
  ~BundledExchangesFactory();

  // Creates a NavigationLoaderInterceptor instance to handle the request for
  // a BundledExchanges, to redirect to the entry URL of the BundledExchanges,
  // and to load the main exchange from the BundledExchanges.
  std::unique_ptr<NavigationLoaderInterceptor> CreateInterceptor();

  // Creates a URLLoaderFactory to load resources from the BundledExchanges.
  void CreateURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory);

 private:
  DISALLOW_COPY_AND_ASSIGN(BundledExchangesFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_FACTORY_H_
