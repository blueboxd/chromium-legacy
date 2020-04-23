// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/is_input_pending_options.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_is_input_pending_options_init.h"

namespace blink {

IsInputPendingOptions* IsInputPendingOptions::Create(
    const IsInputPendingOptionsInit* options_init) {
  return MakeGarbageCollected<IsInputPendingOptions>(
      options_init->includeContinuous());
}

IsInputPendingOptions::IsInputPendingOptions(bool include_continuous)
    : include_continuous_(include_continuous) {}

}  // namespace blink
