// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/surface_texture_gl_owner.h"

#include <memory>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {

SurfaceTextureGLOwner::SurfaceTextureGLOwner(
    std::unique_ptr<gles2::AbstractTexture> texture)
    : TextureOwner(true /*binds_texture_on_update */, std::move(texture)),
      surface_texture_(gl::SurfaceTexture::Create(GetTextureId())),
      context_(gl::GLContext::GetCurrent()),
      surface_(gl::GLSurface::GetCurrent()) {
  DCHECK(context_);
  DCHECK(surface_);
}

SurfaceTextureGLOwner::~SurfaceTextureGLOwner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Clear the texture before we return, so that it can OnTextureDestroyed() if
  // it hasn't already.
  ClearAbstractTexture();
}

void SurfaceTextureGLOwner::OnTextureDestroyed(gles2::AbstractTexture*) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Make sure that the SurfaceTexture isn't using the GL objects.
  surface_texture_ = nullptr;
}

void SurfaceTextureGLOwner::SetFrameAvailableCallback(
    const base::RepeatingClosure& frame_available_cb) {
  DCHECK(!is_frame_available_callback_set_);

  // Setting the callback to be run from any thread since |frame_available_cb|
  // is thread safe.
  is_frame_available_callback_set_ = true;
  surface_texture_->SetFrameAvailableCallbackOnAnyThread(frame_available_cb);
}

gl::ScopedJavaSurface SurfaceTextureGLOwner::CreateJavaSurface() const {
  // |surface_texture_| might be null, but that's okay.
  return gl::ScopedJavaSurface(surface_texture_.get());
}

void SurfaceTextureGLOwner::UpdateTexImage() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (surface_texture_)
    surface_texture_->UpdateTexImage();
}

void SurfaceTextureGLOwner::EnsureTexImageBound() {
  NOTREACHED();
}

void SurfaceTextureGLOwner::GetTransformMatrix(float mtx[]) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // If we don't have a SurfaceTexture, then the matrix doesn't matter.  We
  // still initialize it for good measure.
  if (surface_texture_)
    surface_texture_->GetTransformMatrix(mtx);
  else
    memset(mtx, 0, sizeof(mtx[0]) * 16);
}

void SurfaceTextureGLOwner::ReleaseBackBuffers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (surface_texture_)
    surface_texture_->ReleaseBackBuffers();
}

gl::GLContext* SurfaceTextureGLOwner::GetContext() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return context_.get();
}

gl::GLSurface* SurfaceTextureGLOwner::GetSurface() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return surface_.get();
}

std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
SurfaceTextureGLOwner::GetAHardwareBuffer() {
  NOTREACHED() << "Don't use AHardwareBuffers with SurfaceTextureGLOwner";
  return nullptr;
}

gfx::Rect SurfaceTextureGLOwner::GetCropRect() {
  NOTREACHED() << "Don't use GetCropRect with SurfaceTextureGLOwner";
  return gfx::Rect();
}

void SurfaceTextureGLOwner::GetCodedSizeAndVisibleRect(
    gfx::Size rotated_visible_size,
    gfx::Size* coded_size,
    gfx::Rect* visible_rect) {
  DCHECK(coded_size);
  DCHECK(visible_rect);

  if (!surface_texture_) {
    *visible_rect = gfx::Rect();
    *coded_size = gfx::Size();
    return;
  }

  float mtx[16];
  surface_texture_->GetTransformMatrix(mtx);

  DecomposeTransform(mtx, rotated_visible_size, coded_size, visible_rect);
}

// static
void SurfaceTextureGLOwner::DecomposeTransform(float mtx[16],
                                               gfx::Size rotated_visible_size,
                                               gfx::Size* coded_size,
                                               gfx::Rect* visible_rect) {
  DCHECK(coded_size);
  DCHECK(visible_rect);

  float sx, sy;
  *visible_rect = gfx::Rect();
  // The matrix is in column order and contains transform of (0, 0)x(1, 1)
  // textur coordinates rect into visible portion of the buffer.

  if (mtx[0]) {
    // If mtx[0] is non zero, mtx[5] must be non-zero while mtx[1] and mtx[4]
    // must be zero if this is 0/180 rotation + scale/translate.
    LOG_IF(DFATAL, mtx[1] || mtx[4] || !mtx[5])
        << "Invalid matrix: " << mtx[0] << ", " << mtx[1] << ", " << mtx[4]
        << ", " << mtx[5];

    // Scale is on diagonal, drop any flips or rotations.
    sx = mtx[0];
    sy = mtx[5];

    // 0/180 degrees doesn't swap width/height
    visible_rect->set_size(rotated_visible_size);
  } else {
    // If mtx[0] is zero, mtx[5] must be zero while mtx[1] and mtx[4] must be
    // non-zero if this is rotation 90/270 rotation + scale/translate.
    LOG_IF(DFATAL, !mtx[1] || !mtx[4] || mtx[5])
        << "Invalid matrix: " << mtx[0] << ", " << mtx[1] << ", " << mtx[4]
        << ", " << mtx[5];

    // Scale is on reverse diagonal for inner 2x2 matrix
    sx = mtx[4];
    sy = mtx[1];

    // Frame is rotated, we need to swap width/height
    visible_rect->set_width(rotated_visible_size.height());
    visible_rect->set_height(rotated_visible_size.width());
  }

  // Read translate and flip them if scale is negative.
  float tx = sx > 0 ? mtx[12] : (sx + mtx[12]);
  float ty = sy > 0 ? mtx[13] : (sy + mtx[13]);

  // Drop scale signs from rotation/flip as it's handled above already
  sx = std::abs(sx);
  sy = std::abs(sy);

  *coded_size = visible_rect->size();

  // Note: Below calculation is reverse operation of computing matrix in
  // SurfaceTexture::computeCurrentTransformMatrix() -
  // https://android.googlesource.com/platform/frameworks/native/+/5c1139f/libs/gui/SurfaceTexture.cpp#516.
  // We are assuming here that bilinear filtering is always enabled for
  // sampling the texture.

  // In order to prevent bilinear sampling beyond the edge of the
  // crop rectangle was shrunk by 2 texels in each dimension.  Normally this
  // would just need to take 1/2 a texel off each end, but because the chroma
  // channels of YUV420 images are subsampled we may need to shrink the crop
  // region by a whole texel on each side. As format is not known here we assume
  // the worst case.
  const float shrink_amount = 1.0f;

  // TODO(crbug.com/1076564): Revisit if we need to handle it gracefully.
  CHECK(sx);
  CHECK(sy);

  if (sx < 1.0f) {
    coded_size->set_width(
        std::round((visible_rect->width() - 2.0f * shrink_amount) / sx));
    visible_rect->set_x(std::round(tx * coded_size->width() - shrink_amount));
  }
  if (sy < 1.0f) {
    coded_size->set_height(
        std::round((visible_rect->height() - 2.0f * shrink_amount) / sy));
    visible_rect->set_y(std::round(ty * coded_size->height() - shrink_amount));
  }
}

}  // namespace gpu
