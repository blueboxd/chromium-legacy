// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_texture_wrapper.h"

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "gpu/command_buffer/service/mailbox_manager.h"
#include "media/base/win/mf_helpers.h"
#include "ui/gl/gl_image.h"

namespace media {

// Handy structure so that we can activate / bind one or two textures.
struct ScopedTextureEverything {
  ScopedTextureEverything(GLenum unit, GLuint service_id)
      : active_(unit), binder_(GL_TEXTURE_EXTERNAL_OES, service_id) {}
  ~ScopedTextureEverything() = default;

  // Order is important; we need |active_| to be constructed first
  // and destructed last.
  gl::ScopedActiveTexture active_;
  gl::ScopedTextureBinder binder_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTextureEverything);
};

// Another handy helper class to guarantee that ScopedTextureEverythings
// are deleted in reverse order.  This is required so that the scoped
// active texture unit doesn't change.  Surprisingly, none of the stl
// containers, or the chromium ones, seem to guarantee anything about
// the order of destruction.
struct OrderedDestructionList {
  OrderedDestructionList() = default;
  ~OrderedDestructionList() {
    // Erase last-to-first.
    while (!list_.empty())
      list_.pop_back();
  }

  template <typename... Args>
  void emplace_back(Args&&... args) {
    list_.emplace_back(std::forward<Args>(args)...);
  }

  std::list<ScopedTextureEverything> list_;
  DISALLOW_COPY_AND_ASSIGN(OrderedDestructionList);
};

Texture2DWrapper::Texture2DWrapper() = default;

Texture2DWrapper::~Texture2DWrapper() = default;

DefaultTexture2DWrapper::DefaultTexture2DWrapper(const gfx::Size& size,
                                                 DXGI_FORMAT dxgi_format)
    : size_(size), dxgi_format_(dxgi_format) {}

DefaultTexture2DWrapper::~DefaultTexture2DWrapper() = default;

bool DefaultTexture2DWrapper::ProcessTexture(
    ComD3D11Texture2D texture,
    size_t array_slice,
    const gfx::ColorSpace& input_color_space,
    MailboxHolderArray* mailbox_dest,
    gfx::ColorSpace* output_color_space) {
  // TODO(liberato): When |gpu_resources_| is a SB<>, it's okay to post and
  // forget this call.  It will still be ordered properly with respect to any
  // access on the gpu main thread.
  // TODO(liberato): Would be nice if SB<> knew how to post and reply, so that
  // we could get the error code back eventually, and fail later with it.
  auto result = gpu_resources_->PushNewTexture(std::move(texture), array_slice);
  if (!result.is_ok())
    return false;

  // TODO(liberato): make sure that |mailbox_holders_| is zero-initialized in
  // case we don't use all the planes.
  for (size_t i = 0; i < VideoFrame::kMaxPlanes; i++)
    (*mailbox_dest)[i] = mailbox_holders_[i];

  // We're just binding, so the output and output color spaces are the same.
  *output_color_space = input_color_space;

  return true;
}

bool DefaultTexture2DWrapper::Init(GetCommandBufferHelperCB get_helper_cb) {
  gpu_resources_ = std::make_unique<GpuResources>();
  if (!gpu_resources_)
    return false;

  // YUV textures are mapped onto two GL textures, while RGB use one.
  int textures_per_picture = 0;
  switch (dxgi_format_) {
    case DXGI_FORMAT_NV12:
      textures_per_picture = 2;
      break;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      textures_per_picture = 1;
      break;
    default:
      return false;
  }

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
  // TODO(liberato): Should we use GL_FLOAT for an fp16 texture?  It doesn't
  // really seem to matter so far as I can tell.
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
      // clang-format off
      EGL_CONSUMER_LATENCY_USEC_KHR,         0,
      EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR, 0,
      EGL_NONE,
      // clang-format on
  };
  EGLStreamKHR stream = eglCreateStreamKHR(egl_display, stream_attributes);
  RETURN_ON_FAILURE(!!stream, "Could not create stream", false);

  // |stream| will be destroyed when the GLImage is.
  // TODO(liberato): for tests, it will be destroyed pretty much at the end of
  // this function unless |helper_| retains it.  Also, this won't work if we
  // have a FakeCommandBufferHelper since the service IDs aren't meaningful.
  gl_image_ = base::MakeRefCounted<gl::GLImageDXGI>(size, stream);

  // Bind all the textures so that the stream can find them.
  OrderedDestructionList texture_everythings;
  for (int i = 0; i < textures_per_picture; i++)
    texture_everythings.emplace_back(GL_TEXTURE0 + i, service_ids_[i]);

  std::vector<EGLAttrib> consumer_attributes;
  if (textures_per_picture == 2) {
    // Assume NV12.
    consumer_attributes = {
        // clang-format off
        EGL_COLOR_BUFFER_TYPE,               EGL_YUV_BUFFER_EXT,
        EGL_YUV_NUMBER_OF_PLANES_EXT,        2,
        EGL_YUV_PLANE0_TEXTURE_UNIT_NV,      0,
        EGL_YUV_PLANE1_TEXTURE_UNIT_NV,      1,
        EGL_NONE,
        // clang-format on
    };
  } else {
    // Assume some rgb format.
    consumer_attributes = {
        // clang-format off
        EGL_COLOR_BUFFER_TYPE,               EGL_RGB_BUFFER,
        EGL_NONE,
        // clang-format on
    };
  }
  EGLBoolean result = eglStreamConsumerGLTextureExternalAttribsNV(
      egl_display, stream, consumer_attributes.data());
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

Status DefaultTexture2DWrapper::GpuResources::PushNewTexture(
    ComD3D11Texture2D texture,
    size_t array_slice) {
  if (!helper_ || !helper_->MakeContextCurrent())
    return Status(StatusCode::kCantMakeContextCurrent);

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
    return Status(StatusCode::kCantPostTexture);
  }

  if (!eglStreamConsumerAcquireKHR(egl_display, stream_))
    return Status(StatusCode::kCantPostAcquireStream);

  return OkStatus();
}

}  // namespace media
