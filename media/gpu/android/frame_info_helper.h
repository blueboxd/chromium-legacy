// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_FRAME_INFO_HELPER_H_
#define MEDIA_GPU_ANDROID_FRAME_INFO_HELPER_H_

#include "base/optional.h"
#include "base/threading/sequence_bound.h"
#include "media/gpu/android/codec_image.h"
#include "media/gpu/android/shared_image_video_provider.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// Helper class to fetch YCbCrInfo for Vulkan from a CodecImage.
class MEDIA_GPU_EXPORT FrameInfoHelper {
 public:
  struct FrameInfo {
    FrameInfo();
    ~FrameInfo();

    FrameInfo(FrameInfo&&);
    FrameInfo(const FrameInfo&);
    FrameInfo& operator=(const FrameInfo&);

    gfx::Size coded_size;
    gfx::Rect visible_rect;
    base::Optional<gpu::VulkanYCbCrInfo> ycbcr_info;
  };

  static base::SequenceBound<FrameInfoHelper> Create(
      scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
      SharedImageVideoProvider::GetStubCB get_stub_cb);

  virtual ~FrameInfoHelper() = default;

  // Call |cb| with the FrameInfo.  Will render |buffer_renderer| to the front
  // buffer if we don't have frame info cached. For Vulkan this also will
  // attempt to get YCbCrInfo and cache it.  If all necessary info is cached the
  // call will leave buffer_renderer intact and it can be rendered later.
  // Rendering can fail for reasons. This function will make best efforts to
  // fill FrameInfo which can be used to create VideoFrame, but shouldn't be
  // cached by caller. Last parameter in |cb| is bool that indicates that info
  // is reliable.
  //
  // While this API might seem to be out of its Vulkan mind, it's this
  // complicated to (a) prevent rendering frames out of order to the front
  // buffer, and (b) make it easy to handle the fact that sometimes, we just
  // can't get a YCbCrInfo from a CodecImage due to timeouts.
  virtual void GetFrameInfo(
      std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
      base::OnceCallback<void(std::unique_ptr<CodecOutputBufferRenderer>,
                              FrameInfo,
                              bool)> cb) = 0;

 protected:
  FrameInfoHelper() = default;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_FRAME_INFO_HELPER_H_
