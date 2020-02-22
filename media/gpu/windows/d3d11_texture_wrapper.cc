// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_texture_wrapper.h"

#include <memory>

#include "gpu/command_buffer/service/mailbox_manager.h"
#include "media/base/win/mf_helpers.h"
#include "ui/gl/gl_image.h"

namespace media {

Texture2DWrapper::Texture2DWrapper() = default;

Texture2DWrapper::~Texture2DWrapper() = default;

DefaultTexture2DWrapper::DefaultTexture2DWrapper(const gfx::Size& size)
    : size_(size) {}

DefaultTexture2DWrapper::~DefaultTexture2DWrapper() = default;

bool DefaultTexture2DWrapper::ProcessTexture(ComD3D11Texture2D texture,
                                             size_t array_slice,
                                             MailboxHolderArray* mailbox_dest) {
  // TODO(liberato): When |gpu_resources_| is a SB<>, it's okay to post and
  // forget this call.  It will still be ordered properly with respect to any
  // access on the gpu main thread.
  // TODO(liberato): Would be nice if SB<> knew how to post and reply, so that
  // we could get the error code back.
  auto result = gpu_resources_->PushNewTexture(std::move(texture), array_slice);
  if (!result.IsOk())
    return false;

  for (size_t i = 0; i < VideoFrame::kMaxPlanes; i++)
    (*mailbox_dest)[i] = mailbox_holders_[i];

  return true;
}

bool DefaultTexture2DWrapper::Init(GetCommandBufferHelperCB get_helper_cb) {
  gpu_resources_ = std::make_unique<GpuResources>();
  if (!gpu_resources_)
    return false;

  // We currently only bind NV12, which requires two GL textures.
  const int textures_per_picture = 2;

  // Generate mailboxes and holders.
  std::vector<gpu::Mailbox> mailboxes;
  for (int texture_idx = 0; texture_idx < textures_per_picture; texture_idx++) {
    mailboxes.push_back(gpu::Mailbox::Generate());
    mailbox_holders_[texture_idx] = gpu::MailboxHolder(
        mailboxes[texture_idx], gpu::SyncToken(), GL_TEXTURE_EXTERNAL_OES);
  }

  // Start construction of the GpuResources.
  // We send the texture itself, since we assume that we're using the angle
  // device for decoding.  Sharing seems not to work very well.  Otherwise, we
  // would create the texture with KEYED_MUTEX and NTHANDLE, then send along
  // a handle that we get from |texture| as an IDXGIResource1.
  // TODO(liberato): this should happen on the gpu thread.
  // TODO(liberato): the out param would be handled similarly to
  // CodecImageHolder when we add a pool.
  return gpu_resources_->Init(std::move(get_helper_cb), std::move(mailboxes),
                              GL_TEXTURE_EXTERNAL_OES, size_,
                              textures_per_picture);
}

DefaultTexture2DWrapper::GpuResources::GpuResources() {}

DefaultTexture2DWrapper::GpuResources::~GpuResources() {
  if (helper_ && helper_->MakeContextCurrent()) {
    for (uint32_t service_id : service_ids_)
      helper_->DestroyTexture(service_id);
  }
}

bool DefaultTexture2DWrapper::GpuResources::Init(
    GetCommandBufferHelperCB get_helper_cb,
    const std::vector<gpu::Mailbox> mailboxes,
    GLenum target,
    gfx::Size size,
    int textures_per_picture) {
  helper_ = get_helper_cb.Run();

  if (!helper_ || !helper_->MakeContextCurrent())
    return false;

  // Create the textures and attach them to the mailboxes.
  for (int texture_idx = 0; texture_idx < textures_per_picture; texture_idx++) {
    uint32_t service_id =
        helper_->CreateTexture(target, GL_RGBA, size.width(), size.height(),
                               GL_RGBA, GL_UNSIGNED_BYTE);
    service_ids_.push_back(service_id);
    helper_->ProduceTexture(mailboxes[texture_idx], service_id);
  }

  // Create the stream for zero-copy use by gl.
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  const EGLint stream_attributes[] = {
      EGL_CONSUMER_LATENCY_USEC_KHR,
      0,
      EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR,
      0,
      EGL_NONE,
  };
  EGLStreamKHR stream = eglCreateStreamKHR(egl_display, stream_attributes);
  RETURN_ON_FAILURE(!!stream, "Could not create stream", false);

  // |stream| will be destroyed when the GLImage is.
  // TODO(liberato): for tests, it will be destroyed pretty much at the end of
  // this function unless |helper_| retains it.  Also, this won't work if we
  // have a FakeCommandBufferHelper since the service IDs aren't meaningful.
  gl_image_ = base::MakeRefCounted<gl::GLImageDXGI>(size, stream);
  gl::ScopedActiveTexture texture0(GL_TEXTURE0);
  gl::ScopedTextureBinder texture0_binder(GL_TEXTURE_EXTERNAL_OES,
                                          service_ids_[0]);
  gl::ScopedActiveTexture texture1(GL_TEXTURE1);
  gl::ScopedTextureBinder texture1_binder(GL_TEXTURE_EXTERNAL_OES,
                                          service_ids_[1]);

  EGLAttrib consumer_attributes[] = {
      EGL_COLOR_BUFFER_TYPE,
      EGL_YUV_BUFFER_EXT,
      EGL_YUV_NUMBER_OF_PLANES_EXT,
      2,
      EGL_YUV_PLANE0_TEXTURE_UNIT_NV,
      0,
      EGL_YUV_PLANE1_TEXTURE_UNIT_NV,
      1,
      EGL_NONE,
  };
  EGLBoolean result = eglStreamConsumerGLTextureExternalAttribsNV(
      egl_display, stream, consumer_attributes);
  RETURN_ON_FAILURE(result, "Could not set stream consumer", false);

  EGLAttrib producer_attributes[] = {
      EGL_NONE,
  };

  result = eglCreateStreamProducerD3DTextureANGLE(egl_display, stream,
                                                  producer_attributes);
  RETURN_ON_FAILURE(result, "Could not create stream", false);

  // Note that this is valid as long as |gl_image_| is valid; it is
  // what deletes the stream.
  stream_ = stream;

  // Bind the image to each texture.
  for (size_t texture_idx = 0; texture_idx < service_ids_.size();
       texture_idx++) {
    helper_->BindImage(service_ids_[texture_idx], gl_image_.get(),
                       false /* client_managed */);
  }

  return true;
}

MediaError DefaultTexture2DWrapper::GpuResources::PushNewTexture(
    ComD3D11Texture2D texture,
    size_t array_slice) {
  if (!helper_ || !helper_->MakeContextCurrent())
    return MediaError(ErrorCode::kCannotMakeContextCurrent);

  // Notify |gl_image_| that it has a new texture.
  gl_image_->SetTexture(texture, array_slice);

  // Notify angle that it has a new texture.
  EGLAttrib frame_attributes[] = {
      EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE,
      array_slice,
      EGL_NONE,
  };

  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  if (!eglStreamPostD3DTextureANGLE(egl_display, stream_,
                                    static_cast<void*>(texture.Get()),
                                    frame_attributes)) {
    return MediaError(ErrorCode::kCouldNotPostTexture);
  }

  if (!eglStreamConsumerAcquireKHR(egl_display, stream_))
    return MediaError(ErrorCode::kCouldNotPostAcquireStream);

  return MediaError::Ok();
}

}  // namespace media
