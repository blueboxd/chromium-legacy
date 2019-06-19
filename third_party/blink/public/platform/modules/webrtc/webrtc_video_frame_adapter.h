// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_WEBRTC_WEBRTC_VIDEO_FRAME_ADAPTER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_WEBRTC_WEBRTC_VIDEO_FRAME_ADAPTER_H_

#include <stdint.h>

#include "media/base/video_frame.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/webrtc/api/video/video_frame_buffer.h"

namespace blink {
// Thin adapter from media::VideoFrame to webrtc::VideoFrameBuffer. This
// implementation is read-only and will return null if trying to get a
// non-const pointer to the pixel data. This object will be accessed from
// different threads, but that's safe since it's read-only.
//
// TODO(crbug.com/704136): Move the class below out of the Blink exposed
// API when all users of it have been Onion souped.
class BLINK_PLATFORM_EXPORT WebRtcVideoFrameAdapter
    : public webrtc::VideoFrameBuffer {
 public:
  explicit WebRtcVideoFrameAdapter(scoped_refptr<media::VideoFrame> frame);

  scoped_refptr<media::VideoFrame> getMediaVideoFrame() const { return frame_; }

 private:
  Type type() const override;
  int width() const override;
  int height() const override;

  rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override;
  const webrtc::I420BufferInterface* GetI420() const override;

 protected:
  ~WebRtcVideoFrameAdapter() override;

  rtc::scoped_refptr<webrtc::I420BufferInterface> CreateFrameAdapter() const;

  // Used to cache result of CreateFrameAdapter. Which is called from const
  // GetI420().
  mutable rtc::scoped_refptr<webrtc::I420BufferInterface> frame_adapter_;

  scoped_refptr<media::VideoFrame> frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_WEBRTC_WEBRTC_VIDEO_FRAME_ADAPTER_H_
