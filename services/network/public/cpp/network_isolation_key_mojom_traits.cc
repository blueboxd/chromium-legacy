// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_isolation_key_mojom_traits.h"

namespace mojo {

bool StructTraits<network::mojom::NetworkIsolationKeyDataView,
                  net::NetworkIsolationKey>::
    Read(network::mojom::NetworkIsolationKeyDataView data,
         net::NetworkIsolationKey* out) {
  base::Optional<url::Origin> top_frame_origin, frame_origin;
  if (!data.ReadTopFrameOrigin(&top_frame_origin))
    return false;
  if (!data.ReadFrameOrigin(&frame_origin))
    return false;
  // A key is either fully empty or fully populated (for all fields relevant
  // given the flags set).  The constructor verifies this, so if the top-frame
  // origin is populated, we call the full constructor, otherwise, the empty.
  if (top_frame_origin.has_value()) {
    *out = net::NetworkIsolationKey(top_frame_origin.value(), frame_origin);
  } else {
    *out = net::NetworkIsolationKey();
  }
  return true;
}

}  // namespace mojo
