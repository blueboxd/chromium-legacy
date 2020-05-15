// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/frame_info_helper.h"

#include "gpu/command_buffer/service/shared_image_video.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"

namespace media {

FrameInfoHelper::FrameInfo::FrameInfo() = default;
FrameInfoHelper::FrameInfo::~FrameInfo() = default;
FrameInfoHelper::FrameInfo::FrameInfo(FrameInfo&&) = default;
FrameInfoHelper::FrameInfo::FrameInfo(const FrameInfoHelper::FrameInfo&) =
    default;
FrameInfoHelper::FrameInfo& FrameInfoHelper::FrameInfo::operator=(
    const FrameInfoHelper::FrameInfo&) = default;

// Concrete implementation of FrameInfoHelper that renders output buffers and
// gets the FrameInfo they need.
class FrameInfoHelperImpl : public FrameInfoHelper,
                            public gpu::CommandBufferStub::DestructionObserver {
 public:
  FrameInfoHelperImpl(SharedImageVideoProvider::GetStubCB get_stub_cb) {
    stub_ = get_stub_cb.Run();
    if (stub_)
      stub_->AddDestructionObserver(this);
  }

  ~FrameInfoHelperImpl() override {
    if (stub_)
      stub_->RemoveDestructionObserver(this);
  }

  void GetFrameInfo(
      std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
      base::OnceCallback<
          void(std::unique_ptr<CodecOutputBufferRenderer>, FrameInfo, bool)> cb)
      override {
    if (!buffer_renderer) {
      std::move(cb).Run(nullptr, FrameInfo(), false);
      return;
    }

    auto texture_owner = buffer_renderer->texture_owner();

    FrameInfo info;

    // Indicates that the FrameInfo is reliable and can be cached by caller.
    // It's true if we either return cached values or we attempted to render
    // frame and succeeded.
    bool success = true;

    // We default to visible size if if we can't get real size
    info.coded_size = buffer_renderer->size();
    info.visible_rect = gfx::Rect(info.coded_size);

    if (texture_owner) {
      if (visible_size_ == buffer_renderer->size()) {
        info = frame_info_;
      } else if (buffer_renderer->RenderToTextureOwnerFrontBuffer(
                     CodecOutputBufferRenderer::BindingsMode::
                         kDontRestoreIfBound)) {
        visible_size_ = buffer_renderer->size();
        texture_owner->GetCodedSizeAndVisibleRect(
            visible_size_, &frame_info_.coded_size, &frame_info_.visible_rect);

        frame_info_.ycbcr_info = GetYCbCrInfo(texture_owner.get());
        info = frame_info_;
      } else {
        // We attempted to render frame and failed, mark request as failed so
        // caller won't cache best-guess values.
        success = false;
      }
    }

    std::move(cb).Run(std::move(buffer_renderer), frame_info_, success);
  }

  void OnWillDestroyStub(bool have_context) override {
    DCHECK(stub_);
    stub_ = nullptr;
  }

 private:
  // Gets YCbCrInfo from last rendered frame.
  base::Optional<gpu::VulkanYCbCrInfo> GetYCbCrInfo(
      gpu::TextureOwner* texture_owner) {
    gpu::ContextResult result;
    if (!stub_)
      return base::nullopt;

    auto shared_context =
        stub_->channel()->gpu_channel_manager()->GetSharedContextState(&result);
    auto context_provider =
        (result == gpu::ContextResult::kSuccess) ? shared_context : nullptr;
    if (!context_provider)
      return base::nullopt;

    return gpu::SharedImageVideo::GetYcbcrInfo(texture_owner, context_provider);
  }

  gpu::CommandBufferStub* stub_ = nullptr;

  FrameInfo frame_info_;
  gfx::Size visible_size_;
};

// static
base::SequenceBound<FrameInfoHelper> FrameInfoHelper::Create(
    scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
    SharedImageVideoProvider::GetStubCB get_stub_cb) {
  return base::SequenceBound<FrameInfoHelperImpl>(std::move(gpu_task_runner),
                                                  std::move(get_stub_cb));
}

}  // namespace media
