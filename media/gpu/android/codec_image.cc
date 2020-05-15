// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_image.h"

#include <string.h>

#include <memory>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/bind_helpers.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_make_current.h"

namespace media {

CodecImage::CodecImage() = default;

CodecImage::~CodecImage() {
  NotifyUnused();
}

void CodecImage::Initialize(
    std::unique_ptr<CodecOutputBufferRenderer> output_buffer_renderer,
    scoped_refptr<CodecBufferWaitCoordinator> codec_buffer_wait_coordinator,
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb) {
  DCHECK(output_buffer_renderer);
  output_buffer_renderer_ = std::move(output_buffer_renderer);
  codec_buffer_wait_coordinator_ = std::move(codec_buffer_wait_coordinator);
  promotion_hint_cb_ = std::move(promotion_hint_cb);
}

void CodecImage::AddUnusedCB(UnusedCB unused_cb) {
  unused_cbs_.push_back(std::move(unused_cb));
}

void CodecImage::NotifyUnused() {
  // If we haven't done so yet, release the codec output buffer.  Also drop
  // our reference to the TextureOwner (if any).  In other words, undo anything
  // that we did in Initialize.
  ReleaseCodecBuffer();
  codec_buffer_wait_coordinator_.reset();
  promotion_hint_cb_ = base::NullCallback();

  for (auto& cb : unused_cbs_)
    std::move(cb).Run(this);
  unused_cbs_.clear();
}

gfx::Size CodecImage::GetSize() {
  // Return a nonzero size, to avoid GL errors, even if we dropped the codec
  // buffer already.  Note that if we dropped it, there's no data in the
  // texture anyway, so the old size doesn't matter.
  return output_buffer_renderer_ ? output_buffer_renderer_->size()
                                 : gfx::Size(1, 1);
}

unsigned CodecImage::GetInternalFormat() {
  return GL_RGBA;
}

unsigned CodecImage::GetDataType() {
  return GL_UNSIGNED_BYTE;
}

CodecImage::BindOrCopy CodecImage::ShouldBindOrCopy() {
  // If we're using an overlay, then pretend it's bound.  That way, we'll get
  // calls to ScheduleOverlayPlane.  Otherwise, CopyTexImage needs to be called.
  return !codec_buffer_wait_coordinator_ ? BIND : COPY;
}

bool CodecImage::BindTexImage(unsigned target) {
  DCHECK_EQ(BIND, ShouldBindOrCopy());
  return true;
}

void CodecImage::ReleaseTexImage(unsigned target) {}

bool CodecImage::CopyTexImage(unsigned target) {
  TRACE_EVENT0("media", "CodecImage::CopyTexImage");
  DCHECK_EQ(COPY, ShouldBindOrCopy());

  if (target != GL_TEXTURE_EXTERNAL_OES)
    return false;

  GLint bound_service_id = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &bound_service_id);
  // The currently bound texture should be the texture owner's texture.
  if (bound_service_id !=
      static_cast<GLint>(
          codec_buffer_wait_coordinator_->texture_owner()->GetTextureId()))
    return false;

  if (!output_buffer_renderer_)
    return true;

  output_buffer_renderer_->RenderToTextureOwnerFrontBuffer(
      BindingsMode::kEnsureTexImageBound);
  return true;
}

bool CodecImage::CopyTexSubImage(unsigned target,
                                 const gfx::Point& offset,
                                 const gfx::Rect& rect) {
  return false;
}

bool CodecImage::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int z_order,
    gfx::OverlayTransform transform,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  TRACE_EVENT0("media", "CodecImage::ScheduleOverlayPlane");
  if (codec_buffer_wait_coordinator_) {
    DVLOG(1) << "Invalid call to ScheduleOverlayPlane; this image is "
                "TextureOwner backed.";
    return false;
  }

  NotifyOverlayPromotion(true, bounds_rect);
  RenderToOverlay();
  return true;
}

void CodecImage::NotifyOverlayPromotion(bool promotion,
                                        const gfx::Rect& bounds) {
  // Use-after-release.  It happens if the renderer crashes before getting
  // returns from viz.
  if (!promotion_hint_cb_)
    return;

  if (!codec_buffer_wait_coordinator_ && promotion) {
    // When |CodecImage| is already backed by SurfaceView, and it should be used
    // as overlay.

    // Move the overlay if needed.
    if (most_recent_bounds_ != bounds) {
      most_recent_bounds_ = bounds;
      // Note that, if we're actually promoted to overlay, that this is where
      // the hint is sent to the callback.  NotifyPromotionHint detects this
      // case and lets us do it.  If we knew that we were going to get promotion
      // hints, then we could always let NotifyPromotionHint do it.
      // Unfortunately, we don't know that.
      promotion_hint_cb_.Run(PromotionHintAggregator::Hint(bounds, promotion));
    }
  } else {
    // This could be when |CodecImage| is backed by SurfaceTexture but should be
    // promoted, or when this is backed by either SurfaceView or SurfaceTexture
    // but should not be promoted.
    promotion_hint_cb_.Run(PromotionHintAggregator::Hint(bounds, promotion));
  }
}

void CodecImage::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                              uint64_t process_tracing_id,
                              const std::string& dump_name) {}

void CodecImage::GetTextureMatrix(float matrix[16]) {
  static constexpr float kIdentity[16]{
      1, 0, 0, 0,  //
      0, 1, 0, 0,  //
      0, 0, 1, 0,  //
      0, 0, 0, 1   //
  };
  memcpy(matrix, kIdentity, sizeof(kIdentity));
}

void CodecImage::NotifyPromotionHint(bool promotion_hint,
                                     int display_x,
                                     int display_y,
                                     int display_width,
                                     int display_height) {
  // TODO(crbug.com/1004859): Add back early skip due to suspecting affecting
  // video smoothness.
  if (promotion_hint && !codec_buffer_wait_coordinator_)
    return;

  NotifyOverlayPromotion(
      promotion_hint,
      gfx::Rect(display_x, display_y, display_width, display_height));
}

void CodecImage::ReleaseResources() {
  ReleaseCodecBuffer();
}

bool CodecImage::IsUsingGpuMemory() const {
  if (!output_buffer_renderer_)
    return false;
  // Only the images which are bound to texture accounts for gpu memory.
  return output_buffer_renderer_->was_tex_image_bound();
}

void CodecImage::UpdateAndBindTexImage() {
  RenderToTextureOwnerFrontBuffer(BindingsMode::kEnsureTexImageBound);
}

bool CodecImage::HasTextureOwner() const {
  return !!texture_owner();
}

gpu::TextureBase* CodecImage::GetTextureBase() const {
  return texture_owner()->GetTextureBase();
}

bool CodecImage::RenderToFrontBuffer() {
  if (!output_buffer_renderer_)
    return false;
  return output_buffer_renderer_->RenderToFrontBuffer();
}

bool CodecImage::RenderToTextureOwnerBackBuffer(BlockingMode blocking_mode) {
  if (!output_buffer_renderer_)
    return false;

  return output_buffer_renderer_->RenderToTextureOwnerBackBuffer(blocking_mode);
}

bool CodecImage::RenderToTextureOwnerFrontBuffer(BindingsMode bindings_mode) {
  if (!output_buffer_renderer_)
    return false;
  return output_buffer_renderer_->RenderToTextureOwnerFrontBuffer(
      bindings_mode);
}

bool CodecImage::RenderToOverlay() {
  if (!output_buffer_renderer_)
    return false;
  return output_buffer_renderer_->RenderToOverlay();
}

void CodecImage::ReleaseCodecBuffer() {
  output_buffer_renderer_.reset();
}

std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
CodecImage::GetAHardwareBuffer() {
  // It would be nice if this didn't happen, but we can be incorrectly marked
  // as free when viz is still using us for drawing.  This can happen if the
  // renderer crashes before receiving returns.  It's hard to catch elsewhere,
  // so just handle it gracefully here.
  if (!codec_buffer_wait_coordinator_)
    return nullptr;

  RenderToTextureOwnerFrontBuffer(BindingsMode::kDontRestoreIfBound);
  return codec_buffer_wait_coordinator_->texture_owner()->GetAHardwareBuffer();
}

gfx::Rect CodecImage::GetCropRect() {
  return gfx::Rect();
}

bool CodecImage::HasMutableState() const {
  return false;
}

CodecImageHolder::CodecImageHolder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<CodecImage> codec_image)
    : base::RefCountedDeleteOnSequence<CodecImageHolder>(
          std::move(task_runner)),
      codec_image_(std::move(codec_image)) {}

CodecImageHolder::~CodecImageHolder() = default;

}  // namespace media
