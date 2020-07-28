// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PEERCONNECTION_PEER_CONNECTION_TRACKER_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PEERCONNECTION_PEER_CONNECTION_TRACKER_MOJOM_TRAITS_H_

#include "base/power_monitor/power_observer.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom-shared.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT EnumTraits<blink::mojom::DeviceThermalState,
                                      base::PowerObserver::DeviceThermalState> {
  static blink::mojom::DeviceThermalState ToMojom(
      base::PowerObserver::DeviceThermalState type);

  static bool FromMojom(blink::mojom::DeviceThermalState input,
                        base::PowerObserver::DeviceThermalState* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PEERCONNECTION_PEER_CONNECTION_TRACKER_MOJOM_TRAITS_H_
