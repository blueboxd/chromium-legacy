// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_TRACER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_TRACER_IMPL_H_

#include "third_party/blink/renderer/core/html/media/media_source_tracer.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class HTMLMediaElement;
class MediaSourceImpl;

// Concrete MediaSourceTracer that enables an HTMLMediaElement and its attached
// MediaSourceImpl on the same (main) thread to trace into each other. This
// enables garbage collection to automatically detect and collect idle
// attachments of these objects that have no other strong references.
class MediaSourceTracerImpl final : public MediaSourceTracer {
 public:
  MediaSourceTracerImpl(HTMLMediaElement* media_element,
                        MediaSourceImpl* media_source);
  ~MediaSourceTracerImpl() override = default;

  void Trace(Visitor* visitor) const override;

 private:
  Member<HTMLMediaElement> media_element_;
  Member<MediaSourceImpl> media_source_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_TRACER_IMPL_H_
