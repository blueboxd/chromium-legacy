// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_PLANE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_PLANE_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_handle.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;

class MODULES_EXPORT Plane final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Construct a Plane given a |handle| and a plane index.
  // It is legal for |handle| to be invalid, the resulting Plane will also be
  // invalid.
  Plane(scoped_refptr<VideoFrameHandle> handle, size_t plane);
  ~Plane() override = default;

  // TODO(sandersd): Review return types. There is some implicit casting going
  // on here.
  // TODO(sandersd): Consider throwing if |handle_| is invalidated. Currently
  // everything returns 0, which is not a bad API either.
  uint32_t stride() const;
  uint32_t rows() const;
  uint32_t length() const;

  // Throws InvalidStateError if |handle_| has been invalidated.
  void readInto(MaybeShared<DOMArrayBufferView> dst, ExceptionState&);

 private:
  scoped_refptr<VideoFrameHandle> handle_;
  size_t plane_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_PLANE_H_
