// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/blink_isolate/blink_isolate.h"

namespace blink {

std::unique_ptr<WebIsolate> WebIsolate::Create() {
  return std::unique_ptr<WebIsolate>(new BlinkIsolate());
}

}  // namespace blink
