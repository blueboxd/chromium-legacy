// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_image.h"

#include <string.h>

#include <memory>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_make_current.h"

namespace media {
namespace {

// Makes |texture_owner|'s context current if it isn't already.
std::unique_ptr<ui::ScopedMakeCurrent> MakeCurrentIfNeeded(
    TextureOwner* texture_owner) {
  gl::GLContext* context = texture_owner->GetContext();
  // Note: this works for virtual contexts too, because IsCurrent() returns true
  // if their shared platform context is current, regardless of which virtual
  // context is current.
  if (context->IsCurrent(nullptr))
    return nullptr;

  auto scoped_current = std::make_unique<ui::ScopedMakeCurrent>(
      context, texture_owner->GetSurface());
  // Log an error if ScopedMakeCurrent failed for debugging
  // https://crbug.com/878042.
  // TODO(ericrk): Remove this once debugging is completed.
  if (!context->IsCurrent(nullptr)) {
    LOG(ERROR) << "Failed to make context current in CodecImage. Subsequent "
                  "UpdateTexImage may fail.";
  }
  return scoped_current;
}

}  // namespace

CodecImage::CodecImage() = default;

CodecImage::~CodecImage() {
  if (now_unused_cb_)
    std::move(now_unused_cb_).Run(this);
  if (destruction_cb_)
    std::move(destruction_cb_).Run(this);
}

void CodecImage::Initialize(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    scoped_refptr<TextureOwner> texture_owner,
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb) {
  DCHECK(output_buffer);
  phase_ = Phase::kInCodec;
  output_buffer_ = std::move(output_buffer);
  texture_owner_ = std::move(texture_owner);
  promotion_hint_cb_ = std::move(promotion_hint_cb);
}

void CodecImage::SetNowUnusedCB(NowUnusedCB now_unused_cb) {
  now_unused_cb_ = std::move(now_unused_cb);
}

void CodecImage::SetDestructionCB(DestructionCB destruction_cb) {
  destruction_cb_ = std::move(destruction_cb);
}

gfx::Size CodecImage::GetSize() {
  // Return a nonzero size, to avoid GL errors, even if we dropped the codec
  // buffer already.  Note that if we dropped it, there's no data in the
  // texture anyway, so the old size doesn't matter.
  return output_buffer_ ? output_buffer_->size() : gfx::Size(1, 1);
}

unsigned CodecImage::GetInternalFormat() {
  return GL_RGBA;
}

CodecImage::BindOrCopy CodecImage::ShouldBindOrCopy() {
  // If we're using an overlay, then pretend it's bound.  That way, we'll get
  // calls to ScheduleOverlayPlane.  Otherwise, CopyTexImage needs to be called.
  return !texture_owner_ ? BIND : COPY;
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
  if (bound_service_id != static_cast<GLint>(texture_owner_->GetTextureId()))
    return false;

  RenderToTextureOwnerFrontBuffer(BindingsMode::kEnsureTexImageBound);
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
  if (texture_owner_) {
    DVLOG(1) << "Invalid call to ScheduleOverlayPlane; this image is "
                "TextureOwner backed.";
    return false;
  }

  // Move the overlay if needed.
  if (most_recent_bounds_ != bounds_rect) {
    most_recent_bounds_ = bounds_rect;
    // Note that, if we're actually promoted to overlay, that this is where the
    // hint is sent to the callback.  NotifyPromotionHint detects this case and
    // lets us do it.  If we knew that we were going to get promotion hints,
    // then we could always let NotifyPromotionHint do it.  Unfortunately, we
    // don't know that.
    promotion_hint_cb_.Run(PromotionHintAggregator::Hint(bounds_rect, true));
  }

  RenderToOverlay();
  return true;
}

void CodecImage::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                              uint64_t process_tracing_id,
                              const std::string& dump_name) {}

void CodecImage::GetTextureMatrix(float matrix[16]) {
  // Default to identity.
  static constexpr float kYInvertedIdentity[16]{
      1, 0,  0, 0,  //
      0, -1, 0, 0,  //
      0, 0,  1, 0,  //
      0, 1,  0, 1   //
  };
  memcpy(matrix, kYInvertedIdentity, sizeof(kYInvertedIdentity));
  if (!texture_owner_)
    return;

  // The matrix is available after we render to the front buffer. If that fails
  // we'll return the matrix from the previous frame, which is more likely to be
  // correct than the identity matrix anyway.
  RenderToTextureOwnerFrontBuffer(BindingsMode::kDontRestoreIfBound);
  texture_owner_->GetTransformMatrix(matrix);
  YInvertMatrix(matrix);
}

void CodecImage::NotifyPromotionHint(bool promotion_hint,
                                     int display_x,
                                     int display_y,
                                     int display_width,
                                     int display_height) {
  // If this is promotable, and we're using an overlay, then skip sending this
  // hint.  ScheduleOverlayPlane will do it.
  if (promotion_hint && !texture_owner_)
    return;

  promotion_hint_cb_.Run(PromotionHintAggregator::Hint(
      gfx::Rect(display_x, display_y, display_width, display_height),
      promotion_hint));
}

bool CodecImage::RenderToFrontBuffer() {
  // This code is used to trigger early rendering of the image before it is used
  // for compositing, there is no need to bind the image.
  return texture_owner_
             ? RenderToTextureOwnerFrontBuffer(BindingsMode::kRestoreIfBound)
             : RenderToOverlay();
}

bool CodecImage::RenderToTextureOwnerBackBuffer() {
  DCHECK(texture_owner_);
  DCHECK_NE(phase_, Phase::kInFrontBuffer);
  if (phase_ == Phase::kInBackBuffer)
    return true;
  if (phase_ == Phase::kInvalidated)
    return false;

  // Wait for a previous frame available so we don't confuse it with the one
  // we're about to release.
  if (texture_owner_->IsExpectingFrameAvailable())
    texture_owner_->WaitForFrameAvailable();
  if (!output_buffer_->ReleaseToSurface()) {
    phase_ = Phase::kInvalidated;
    return false;
  }
  phase_ = Phase::kInBackBuffer;
  texture_owner_->SetReleaseTimeToNow();
  return true;
}

bool CodecImage::RenderToTextureOwnerFrontBuffer(BindingsMode bindings_mode) {
  DCHECK(texture_owner_);

  if (phase_ == Phase::kInFrontBuffer) {
    EnsureBoundIfNeeded(bindings_mode);
    return true;
  }
  if (phase_ == Phase::kInvalidated)
    return false;

  // Render it to the back buffer if it's not already there.
  if (!RenderToTextureOwnerBackBuffer())
    return false;

  // The image is now in the back buffer, so promote it to the front buffer.
  phase_ = Phase::kInFrontBuffer;
  if (texture_owner_->IsExpectingFrameAvailable())
    texture_owner_->WaitForFrameAvailable();

  std::unique_ptr<ui::ScopedMakeCurrent> scoped_make_current =
      MakeCurrentIfNeeded(texture_owner_.get());
  // If updating the image will implicitly update the texture bindings then
  // restore if requested or the update needed a context switch.
  bool should_restore_bindings =
      texture_owner_->binds_texture_on_update() &&
      (bindings_mode == BindingsMode::kRestoreIfBound || !!scoped_make_current);

  GLint bound_service_id = 0;
  if (should_restore_bindings)
    glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &bound_service_id);
  texture_owner_->UpdateTexImage();
  EnsureBoundIfNeeded(bindings_mode);
  if (should_restore_bindings)
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, bound_service_id);
  return true;
}

void CodecImage::EnsureBoundIfNeeded(BindingsMode mode) {
  DCHECK(texture_owner_);

  if (texture_owner_->binds_texture_on_update()) {
    was_tex_image_bound_ = true;
    return;
  }
  if (mode != BindingsMode::kEnsureTexImageBound)
    return;
  texture_owner_->EnsureTexImageBound();
  was_tex_image_bound_ = true;
}

bool CodecImage::RenderToOverlay() {
  if (phase_ == Phase::kInFrontBuffer)
    return true;
  if (phase_ == Phase::kInvalidated)
    return false;

  if (!output_buffer_->ReleaseToSurface()) {
    phase_ = Phase::kInvalidated;
    return false;
  }
  phase_ = Phase::kInFrontBuffer;
  return true;
}

void CodecImage::ReleaseCodecBuffer() {
  output_buffer_ = nullptr;
  phase_ = Phase::kInvalidated;
}

std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
CodecImage::GetAHardwareBuffer() {
  DCHECK(texture_owner_);

  RenderToTextureOwnerFrontBuffer(BindingsMode::kDontRestoreIfBound);
  return texture_owner_->GetAHardwareBuffer();
}

CodecImageHolder::CodecImageHolder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<CodecImage> codec_image)
    : base::RefCountedDeleteOnSequence<CodecImageHolder>(
          std::move(task_runner)),
      codec_image_(std::move(codec_image)) {}

CodecImageHolder::~CodecImageHolder() = default;

}  // namespace media
