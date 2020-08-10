// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_PLANE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_PLANE_H_

#include <stdint.h>

#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class ExceptionState;

class MODULES_EXPORT Plane final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~Plane() override = default;

  uint32_t stride() const;
  uint32_t rows() const;
  uint32_t length() const;

  void readInto(MaybeShared<DOMArrayBufferView> dst, ExceptionState&);

  void Trace(Visitor*) const override;

 private:
  Plane(VideoFrame* frame, uint32_t plane);

  Member<VideoFrame> frame_;
  uint32_t plane_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_PLANE_H_
