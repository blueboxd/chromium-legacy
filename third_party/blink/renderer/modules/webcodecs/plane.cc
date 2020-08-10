// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/plane.h"

namespace blink {

Plane::Plane(VideoFrame* frame, uint32_t plane)
    : frame_(frame), plane_(plane) {}

uint32_t Plane::stride() const {
  // Use |plane_|, to satisfy the compiler.
  static_cast<void>(plane_);

  // TODO(sandersd): Look up via |frame_|.
  return 0;
}

uint32_t Plane::rows() const {
  // TODO(sandersd): Look up via |frame_|.
  return 0;
}

uint32_t Plane::length() const {
  // TODO(sandersd): Look up via |frame_|.
  return 0;
}

void Plane::readInto(MaybeShared<DOMArrayBufferView> dst, ExceptionState&) {}

void Plane::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
