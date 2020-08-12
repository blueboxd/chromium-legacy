// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_ATTACHMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_ATTACHMENT_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"

namespace blink {

// Used to serialize video frames without copying frame data.
class MODULES_EXPORT VideoFrameAttachment
    : public SerializedScriptValue::Attachment {
 public:
  static const void* const kAttachmentKey;
  VideoFrameAttachment() = default;
  ~VideoFrameAttachment() override = default;

  bool IsLockedToAgentCluster() const override {
    return !frame_handles_.IsEmpty();
  }

  size_t size() const { return frame_handles_.size(); }

  Vector<scoped_refptr<VideoFrame::Handle>>& Handles() {
    return frame_handles_;
  }

  const Vector<scoped_refptr<VideoFrame::Handle>>& Handles() const {
    return frame_handles_;
  }

 private:
  Vector<scoped_refptr<VideoFrame::Handle>> frame_handles_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_ATTACHMENT_H_
