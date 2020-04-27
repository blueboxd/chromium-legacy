// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/isolation_info_mojom_traits.h"

#include "base/notreached.h"
#include "services/network/public/cpp/site_for_cookies_mojom_traits.h"

namespace mojo {

bool EnumTraits<network::mojom::IsolationInfoRedirectMode,
                net::IsolationInfo::RedirectMode>::
    FromMojom(network::mojom::IsolationInfoRedirectMode redirect_mode,
              net::IsolationInfo::RedirectMode* out) {
  switch (redirect_mode) {
    case network::mojom::IsolationInfoRedirectMode::kUpdateTopFrame:
      *out = net::IsolationInfo::RedirectMode::kUpdateTopFrame;
      return true;
    case network::mojom::IsolationInfoRedirectMode::kUpdateFrameOnly:
      *out = net::IsolationInfo::RedirectMode::kUpdateFrameOnly;
      return true;
    case network::mojom::IsolationInfoRedirectMode::kUpdateNothing:
      *out = net::IsolationInfo::RedirectMode::kUpdateNothing;
      return true;
  }
  return false;
}

network::mojom::IsolationInfoRedirectMode EnumTraits<
    network::mojom::IsolationInfoRedirectMode,
    net::IsolationInfo::RedirectMode>::ToMojom(net::IsolationInfo::RedirectMode
                                                   redirect_mode) {
  switch (redirect_mode) {
    case net::IsolationInfo::RedirectMode::kUpdateTopFrame:
      return network::mojom::IsolationInfoRedirectMode::kUpdateTopFrame;
    case net::IsolationInfo::RedirectMode::kUpdateFrameOnly:
      return network::mojom::IsolationInfoRedirectMode::kUpdateFrameOnly;
    case net::IsolationInfo::RedirectMode::kUpdateNothing:
      return network::mojom::IsolationInfoRedirectMode::kUpdateNothing;
  }

  NOTREACHED();
  return network::mojom::IsolationInfoRedirectMode::kUpdateNothing;
}

bool StructTraits<network::mojom::IsolationInfoDataView, net::IsolationInfo>::
    Read(network::mojom::IsolationInfoDataView data, net::IsolationInfo* out) {
  base::Optional<url::Origin> top_frame_origin;
  base::Optional<url::Origin> frame_origin;
  net::SiteForCookies site_for_cookies;
  net::IsolationInfo::RedirectMode redirect_mode;

  if (!data.ReadTopFrameOrigin(&top_frame_origin) ||
      !data.ReadFrameOrigin(&frame_origin) ||
      !data.ReadSiteForCookies(&site_for_cookies) ||
      !data.ReadRedirectMode(&redirect_mode)) {
    return false;
  }

  base::Optional<net::IsolationInfo> isolation_info =
      net::IsolationInfo::CreateIfConsistent(redirect_mode, top_frame_origin,
                                             frame_origin, site_for_cookies,
                                             data.opaque_and_non_transient());
  if (!isolation_info)
    return false;

  *out = std::move(*isolation_info);
  return true;
}

}  // namespace mojo
