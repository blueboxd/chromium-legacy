// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/site_for_cookies_mojom_traits.h"
#include "net/base/features.h"

namespace mojo {

bool StructTraits<network::mojom::SiteForCookiesDataView, net::SiteForCookies>::
    Read(network::mojom::SiteForCookiesDataView data,
         net::SiteForCookies* out) {
  std::string scheme, registrable_domain;
  if (!data.ReadScheme(&scheme))
    return false;
  if (!data.ReadRegistrableDomain(&registrable_domain))
    return false;

  return net::SiteForCookies::FromWire(scheme, registrable_domain, out);
}

}  // namespace mojo
