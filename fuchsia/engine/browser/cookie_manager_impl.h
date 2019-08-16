// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_COOKIE_MANAGER_IMPL_H_
#define FUCHSIA_ENGINE_BROWSER_COOKIE_MANAGER_IMPL_H_

#include <fuchsia/web/cpp/fidl.h>

#include "base/callback.h"
#include "base/macros.h"
#include "fuchsia/engine/web_engine_export.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

class WEB_ENGINE_EXPORT CookieManagerImpl : public fuchsia::web::CookieManager {
 public:
  // Used to request the BrowserContext's CookieManager. Since the
  // NetworkContext may change, e.g. if the NetworkService crashes, the returned
  // pointer will be used immediately, and not cached by the CookieManagerImpl.
  using GetNetworkContextCallback =
      base::RepeatingCallback<network::mojom::NetworkContext*()>;

  // |get_network_context| will be called to (re)connect to CookieManager,
  // on-demand, in response to query/observation requests.
  explicit CookieManagerImpl(GetNetworkContextCallback get_network_context);
  ~CookieManagerImpl() final;

  // fuchsia::web::CookieManager implementation:
  void ObserveCookieChanges(
      fidl::StringPtr url,
      fidl::StringPtr name,
      fidl::InterfaceRequest<fuchsia::web::CookiesIterator> changes) final;
  void GetCookieList(
      fidl::StringPtr url,
      fidl::StringPtr name,
      fidl::InterfaceRequest<fuchsia::web::CookiesIterator> cookies) final;

 private:
  // (Re)connects |cookie_manager_| if not currently connected.
  void EnsureCookieManager();

  // Handles errors on the |cookie_manager_| Mojo channel.
  void OnMojoError();

  const GetNetworkContextCallback get_network_context_;
  network::mojom::CookieManagerPtr cookie_manager_;

  DISALLOW_COPY_AND_ASSIGN(CookieManagerImpl);
};

#endif  // FUCHSIA_ENGINE_BROWSER_COOKIE_MANAGER_IMPL_H_
