// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_ISOLATION_KEY_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_ISOLATION_KEY_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/mojom/network_isolation_key.mojom-shared.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/origin.h"

namespace mojo {

template <>
struct StructTraits<network::mojom::NetworkIsolationKeyDataView,
                    net::NetworkIsolationKey> {
  static bool IsNull(const net::NetworkIsolationKey& input) {
    return input.IsEmpty();
  }

  static void SetToNull(net::NetworkIsolationKey* out) {
    *out = net::NetworkIsolationKey();
  }

  static const base::Optional<url::Origin>& top_frame_origin(
      const net::NetworkIsolationKey& input) {
    return input.GetTopFrameOrigin();
  }

  static bool Read(network::mojom::NetworkIsolationKeyDataView data,
                   net::NetworkIsolationKey* out) {
    base::Optional<url::Origin> top_frame_origin;
    if (!data.ReadTopFrameOrigin(&top_frame_origin))
      return false;
    *out = net::NetworkIsolationKey(top_frame_origin);
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_ISOLATION_KEY_MOJOM_TRAITS_H_
