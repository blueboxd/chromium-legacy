// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/blink_isolate/blink_isolate.h"

namespace blink {

// TODO(crbug.com/1051395) Actually instantiate multiple blink isolates.
static BlinkIsolate* g_single_isolate = nullptr;

BlinkIsolate* BlinkIsolate::GetCurrent() {
  return g_single_isolate;
}

BlinkIsolate::BlinkIsolate() {
  g_single_isolate = this;
}

BlinkIsolate::~BlinkIsolate() = default;

}  // namespace blink
