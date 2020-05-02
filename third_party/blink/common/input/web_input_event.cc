// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/input/web_input_event.h"

namespace blink {

WebInputEvent::DispatchType WebInputEvent::MergeDispatchTypes(
    DispatchType type_1,
    DispatchType type_2) {
  static_assert(DispatchType::kBlocking < DispatchType::kEventNonBlocking,
                "Enum not ordered correctly");
  static_assert(DispatchType::kEventNonBlocking <
                    DispatchType::kListenersNonBlockingPassive,
                "Enum not ordered correctly");
  static_assert(DispatchType::kListenersNonBlockingPassive <
                    DispatchType::kListenersForcedNonBlockingDueToFling,
                "Enum not ordered correctly");
  return std::min(type_1, type_2);
}

}  // namespace blink
