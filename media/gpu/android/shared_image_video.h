// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_SHARED_IMAGE_VIDEO_H_
#define MEDIA_GPU_ANDROID_SHARED_IMAGE_VIDEO_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "media/gpu/media_gpu_export.h"

namespace gpu {
class SharedImageRepresentationGLTexture;
class SharedImageRepresentationSkia;
struct Mailbox;

namespace gles2 {
class AbstractTexture;
}  // namespace gles2

}  // namespace gpu

namespace media {
class CodecImage;

// Implementation of SharedImageBacking that renders MediaCodec buffers to a
// TextureOwner or overlay as needed in order to draw them.
class MEDIA_GPU_EXPORT SharedImageVideo
    : public gpu::SharedImageBacking,
      public gpu::SharedContextState::ContextLostObserver {
 public:
  SharedImageVideo(
      const gpu::Mailbox& mailbox,
      const gfx::ColorSpace color_space,
      scoped_refptr<CodecImage> codec_image,
      std::unique_ptr<gpu::gles2::AbstractTexture> abstract_texture,
      scoped_refptr<gpu::SharedContextState> shared_context_state,
      bool is_thread_safe);

  ~SharedImageVideo() override;

  // SharedImageBacking implementation.
  bool IsCleared() const override;
  void SetCleared() override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool ProduceLegacyMailbox(gpu::MailboxManager* mailbox_manager) override;
  void Destroy() override;
  size_t EstimatedSizeForMemTracking() const override;

  // SharedContextState::ContextLostObserver implementation.
  void OnContextLost() override;

  // Returns ycbcr information. This is only valid in vulkan context and
  // nullopt for other context.
  base::Optional<gpu::VulkanYCbCrInfo> GetYcbcrInfo();

 protected:
  std::unique_ptr<gpu::SharedImageRepresentationGLTexture> ProduceGLTexture(
      gpu::SharedImageManager* manager,
      gpu::MemoryTypeTracker* tracker) override;

  std::unique_ptr<gpu::SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(gpu::SharedImageManager* manager,
                              gpu::MemoryTypeTracker* tracker) override;

  std::unique_ptr<gpu::SharedImageRepresentationSkia> ProduceSkia(
      gpu::SharedImageManager* manager,
      gpu::MemoryTypeTracker* tracker,
      scoped_refptr<gpu::SharedContextState> context_state) override;

  // TODO(vikassoni): Add overlay and AHardwareBuffer representations in future
  // patch. Overlays are anyways using legacy mailbox for now.

 private:
  friend class SharedImageRepresentationGLTextureVideo;
  friend class SharedImageRepresentationGLTexturePassthroughVideo;
  friend class SharedImageRepresentationVideoSkiaGL;
  friend class SharedImageRepresentationVideoSkiaVk;

  void BeginGLReadAccess();

  scoped_refptr<CodecImage> codec_image_;

  // |abstract_texture_| is only used for legacy mailbox.
  std::unique_ptr<gpu::gles2::AbstractTexture> abstract_texture_;
  scoped_refptr<gpu::SharedContextState> context_state_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageVideo);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_SHARED_IMAGE_VIDEO_H_
