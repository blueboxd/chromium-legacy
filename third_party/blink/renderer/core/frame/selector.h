// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SELECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SELECTOR_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

// TODO(crbug/1000308): Implement the Selector type. This member currently
// serves as a feature detectable API for the Text Fragment Identifiers
// feature.
class Selector : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SELECTOR_H_
