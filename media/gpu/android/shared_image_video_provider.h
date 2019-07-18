// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_SHARED_IMAGE_VIDEO_PROVIDER_H_
#define MEDIA_GPU_ANDROID_SHARED_IMAGE_VIDEO_PROVIDER_H_

#include "base/callback.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "media/gpu/android/codec_image_group.h"
#include "media/gpu/android/promotion_hint_aggregator.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
class CommandBufferStub;
class SharedContextState;
struct SyncToken;
}  // namespace gpu

namespace media {

// Provider class for shared images.
class MEDIA_GPU_EXPORT SharedImageVideoProvider {
 public:
  using GetStubCB = base::RepeatingCallback<gpu::CommandBufferStub*()>;
  using GpuInitCB =
      base::OnceCallback<void(scoped_refptr<gpu::SharedContextState>)>;

  // Description of the underlying properties of the shared image.
  struct ImageSpec {
    ImageSpec(const gfx::Size& size);
    ImageSpec(const ImageSpec&);
    ~ImageSpec();

    // Size of the underlying texture.
    gfx::Size size;

    // TODO: Include other properties, if they matter, like texture format.

    bool operator==(const ImageSpec&);
  };

  using ReleaseCB = base::OnceCallback<void(const gpu::SyncToken&)>;

  // Description of the image that's being provided to the client.
  struct ImageRecord {
    ImageRecord();
    ImageRecord(ImageRecord&&);
    ~ImageRecord();

    // Mailbox to which this shared image is bound.
    gpu::Mailbox mailbox;

    // Sampler conversion information which is used in vulkan context.
    base::Optional<gpu::VulkanYCbCrInfo> ycbcr_info;

    // Release callback.  When this is called (or dropped), the image will be
    // considered to be unused.
    ReleaseCB release_cb;

    // CodecImage that one can use for MaybeRenderEarly.
    scoped_refptr<CodecImageHolder> codec_image_holder;

   private:
    DISALLOW_COPY_AND_ASSIGN(ImageRecord);
  };

  SharedImageVideoProvider() = default;
  virtual ~SharedImageVideoProvider() = default;

  using ImageReadyCB = base::OnceCallback<void(ImageRecord)>;

  // Initialize this provider.  On success, |gpu_init_cb| will be run with the
  // SharedContextState (and the context current), on the gpu main thread.  This
  // is mostly a hack to allow VideoFrameFactoryImpl to create a TextureOwner on
  // the right context.  Will call |gpu_init_cb| with nullptr otherwise.
  virtual void Initialize(GpuInitCB gpu_init_cb) = 0;

  // Call |cb| when we have a shared image that matches |spec|.  We may call
  // |cb| back before returning, or we might post it for later.
  virtual void RequestImage(ImageReadyCB cb,
                            const ImageSpec& spec,
                            scoped_refptr<TextureOwner> texture_owner) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedImageVideoProvider);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_SHARED_IMAGE_VIDEO_PROVIDER_H_
