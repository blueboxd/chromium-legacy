// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_picture_native_pixmap_angle.h"

#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/base/ui_base_features.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_egl_pixmap.h"
#include "ui/gl/scoped_binders.h"

namespace media {

namespace {

inline Pixmap CreatePixmap(const gfx::Size& size) {
  auto* display = gfx::GetXDisplay();
  if (!display)
    return 0;

  int screen = DefaultScreen(display);
  auto root = XRootWindow(display, screen);
  if (root == BadValue)
    return 0;

  XWindowAttributes win_attr = {};
  // returns 0 on failure, see:
  // https://tronche.com/gui/x/xlib/introduction/errors.html#Status
  if (!XGetWindowAttributes(display, root, &win_attr))
    return 0;

  // TODO(tmathmeyer) should we use the depth from libva instead of root window?
  return XCreatePixmap(display, root, size.width(), size.height(),
                       win_attr.depth);
}

}  // namespace

VaapiPictureNativePixmapAngle::VaapiPictureNativePixmapAngle(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    int32_t picture_buffer_id,
    const gfx::Size& size,
    const gfx::Size& visible_size,
    uint32_t service_texture_id,
    uint32_t client_texture_id,
    uint32_t texture_target)
    : VaapiPictureNativePixmap(std::move(vaapi_wrapper),
                               make_context_current_cb,
                               bind_image_cb,
                               picture_buffer_id,
                               size,
                               visible_size,
                               service_texture_id,
                               client_texture_id,
                               texture_target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check that they're both not 0
  DCHECK(service_texture_id);
  DCHECK(client_texture_id);
}

VaapiPictureNativePixmapAngle::~VaapiPictureNativePixmapAngle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (gl_image_ && make_context_current_cb_.Run()) {
    gl_image_->ReleaseTexImage(texture_target_);
    DCHECK_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  }

  if (x_pixmap_) {
    if (auto* display = gfx::GetXDisplay()) {
      XFreePixmap(display, x_pixmap_);
    }
  }
}

Status VaapiPictureNativePixmapAngle::Allocate(gfx::BufferFormat format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!(texture_id_ || client_texture_id_))
    return StatusCode::kVaapiNoTexture;

  if (!make_context_current_cb_ || !make_context_current_cb_.Run())
    return StatusCode::kVaapiBadContext;

  DCHECK(!features::IsUsingOzonePlatform());
  auto image =
      base::MakeRefCounted<gl::GLImageEGLPixmap>(visible_size_, format);
  if (!image)
    return StatusCode::kVaapiNoImage;

  x_pixmap_ = CreatePixmap(visible_size_);
  if (!x_pixmap_)
    return StatusCode::kVaapiNoPixmap;

  if (!image->Initialize(x_pixmap_))
    return StatusCode::kVaapiFailedToInitializeImage;

  gl::ScopedTextureBinder texture_binder(texture_target_, texture_id_);
  if (!image->BindTexImage(texture_target_))
    return StatusCode::kVaapiFailedToBindTexture;

  gl_image_ = image;

  DCHECK(bind_image_cb_);
  if (!bind_image_cb_.Run(client_texture_id_, texture_target_, gl_image_,
                          /*can_bind_to_sampler=*/true)) {
    return StatusCode::kVaapiFailedToBindImage;
  }

  return OkStatus();
}

bool VaapiPictureNativePixmapAngle::ImportGpuMemoryBufferHandle(
    gfx::BufferFormat format,
    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) {
  NOTREACHED();
  return false;
}

bool VaapiPictureNativePixmapAngle::DownloadFromSurface(
    scoped_refptr<VASurface> va_surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!make_context_current_cb_ || !make_context_current_cb_.Run())
    return false;

  DCHECK(texture_id_);
  gl::ScopedTextureBinder texture_binder(texture_target_, texture_id_);

  // GL needs to re-bind the texture after the pixmap content is updated so that
  // the compositor sees the updated contents (we found this out experimentally)
  gl_image_->ReleaseTexImage(texture_target_);

  DCHECK(gfx::Rect(va_surface->size()).Contains(gfx::Rect(visible_size_)));
  if (!vaapi_wrapper_->PutSurfaceIntoPixmap(va_surface->id(), x_pixmap_,
                                            visible_size_)) {
    return false;
  }
  return gl_image_->BindTexImage(texture_target_);
}

VASurfaceID VaapiPictureNativePixmapAngle::va_surface_id() const {
  return VA_INVALID_ID;
}

}  // namespace media
