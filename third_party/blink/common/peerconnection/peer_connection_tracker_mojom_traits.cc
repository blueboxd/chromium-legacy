// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/peerconnection/peer_connection_tracker_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

// static
blink::mojom::DeviceThermalState
EnumTraits<blink::mojom::DeviceThermalState,
           base::PowerObserver::DeviceThermalState>::
    ToMojom(base::PowerObserver::DeviceThermalState type) {
  switch (type) {
    case base::PowerObserver::DeviceThermalState::kUnknown:
      return blink::mojom::DeviceThermalState::kUnknown;
    case base::PowerObserver::DeviceThermalState::kNominal:
      return blink::mojom::DeviceThermalState::kNominal;
    case base::PowerObserver::DeviceThermalState::kFair:
      return blink::mojom::DeviceThermalState::kFair;
    case base::PowerObserver::DeviceThermalState::kSerious:
      return blink::mojom::DeviceThermalState::kSerious;
    case base::PowerObserver::DeviceThermalState::kCritical:
      return blink::mojom::DeviceThermalState::kCritical;
  }
  NOTREACHED();
  return blink::mojom::DeviceThermalState::kUnknown;
}

// static
bool EnumTraits<blink::mojom::DeviceThermalState,
                base::PowerObserver::DeviceThermalState>::
    FromMojom(blink::mojom::DeviceThermalState input,
              base::PowerObserver::DeviceThermalState* out) {
  switch (input) {
    case blink::mojom::DeviceThermalState::kUnknown:
      *out = base::PowerObserver::DeviceThermalState::kUnknown;
      return true;
    case blink::mojom::DeviceThermalState::kNominal:
      *out = base::PowerObserver::DeviceThermalState::kNominal;
      return true;
    case blink::mojom::DeviceThermalState::kFair:
      *out = base::PowerObserver::DeviceThermalState::kFair;
      return true;
    case blink::mojom::DeviceThermalState::kSerious:
      *out = base::PowerObserver::DeviceThermalState::kSerious;
      return true;
    case blink::mojom::DeviceThermalState::kCritical:
      *out = base::PowerObserver::DeviceThermalState::kCritical;
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace mojo
