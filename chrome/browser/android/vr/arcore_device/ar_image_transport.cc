// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/ar_image_transport.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/containers/queue.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "chrome/browser/android/vr/mailbox_to_surface_bridge.h"
#include "chrome/browser/android/vr/web_xr_presentation_state.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_android_hardware_buffer.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace device {

ArImageTransport::ArImageTransport(
    std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge)
    : gl_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      mailbox_bridge_(std::move(mailbox_bridge)) {}

ArImageTransport::~ArImageTransport() {
  DCHECK(IsOnGlThread());

  if (webxr_) {
    std::vector<std::unique_ptr<vr::WebXrSharedBuffer>> buffers =
        webxr_->TakeSharedBuffers();
    for (auto& buffer : buffers) {
      if (!buffer->mailbox_holder.mailbox.IsZero()) {
        DCHECK(mailbox_bridge_);
        DVLOG(2) << ": DestroySharedImage, mailbox="
                 << buffer->mailbox_holder.mailbox.ToDebugString();
        // Note: the sync token in mailbox_holder may not be accurate. See
        // comment in TransferFrame below.
        mailbox_bridge_->DestroySharedImage(buffer->mailbox_holder);
      }
    }
  }
}

bool ArImageTransport::Initialize(vr::WebXrPresentationState* webxr) {
  DVLOG(1) << __func__;
  DCHECK(IsOnGlThread());

  webxr_ = webxr;

  mailbox_bridge_->BindContextProviderToCurrentThread();

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  ar_renderer_ = std::make_unique<ArRenderer>();
  glGenTextures(1, &camera_texture_id_arcore_);

  glGenFramebuffersEXT(1, &camera_fbo_);

  return true;
}

GLuint ArImageTransport::GetCameraTextureId() {
  return camera_texture_id_arcore_;
}

void ArImageTransport::ResizeSharedBuffer(const gfx::Size& size,
                                          vr::WebXrSharedBuffer* buffer) {
  DCHECK(IsOnGlThread());

  if (buffer->size == size)
    return;

  TRACE_EVENT0("gpu", __FUNCTION__);
  // Unbind previous image (if any).
  if (!buffer->mailbox_holder.mailbox.IsZero()) {
    DVLOG(2) << ": DestroySharedImage, mailbox="
             << buffer->mailbox_holder.mailbox.ToDebugString();
    // Note: the sync token in mailbox_holder may not be accurate. See comment
    // in TransferFrame below.
    mailbox_bridge_->DestroySharedImage(buffer->mailbox_holder);
  }

  DVLOG(2) << __FUNCTION__ << ": width=" << size.width()
           << " height=" << size.height();
  // Remove reference to previous image (if any).
  buffer->local_glimage = nullptr;

  static constexpr gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
  static constexpr gfx::BufferUsage usage = gfx::BufferUsage::SCANOUT;

  gfx::GpuMemoryBufferId kBufferId(webxr_->next_memory_buffer_id++);
  buffer->gmb = gpu::GpuMemoryBufferImplAndroidHardwareBuffer::Create(
      kBufferId, size, format, usage,
      gpu::GpuMemoryBufferImpl::DestructionCallback());

  uint32_t shared_image_usage = gpu::SHARED_IMAGE_USAGE_SCANOUT |
                                gpu::SHARED_IMAGE_USAGE_DISPLAY |
                                gpu::SHARED_IMAGE_USAGE_GLES2;
  buffer->mailbox_holder = mailbox_bridge_->CreateSharedImage(
      buffer->gmb.get(), gfx::ColorSpace(), shared_image_usage);
  DVLOG(2) << ": CreateSharedImage, mailbox="
           << buffer->mailbox_holder.mailbox.ToDebugString();

  auto img = base::MakeRefCounted<gl::GLImageAHardwareBuffer>(size);

  base::android::ScopedHardwareBufferHandle ahb =
      buffer->gmb->CloneHandle().android_hardware_buffer;
  bool ret = img->Initialize(ahb.get(), false /* preserved */);
  if (!ret) {
    DLOG(WARNING) << __FUNCTION__ << ": ERROR: failed to initialize image!";
    return;
  }
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, buffer->local_texture);
  img->BindTexImage(GL_TEXTURE_EXTERNAL_OES);
  buffer->local_glimage = std::move(img);

  // Save size to avoid resize next time.
  DVLOG(1) << __FUNCTION__ << ": resized to " << size.width() << "x"
           << size.height();
  buffer->size = size;
}

std::unique_ptr<vr::WebXrSharedBuffer> ArImageTransport::CreateBuffer() {
  std::unique_ptr<vr::WebXrSharedBuffer> buffer =
      std::make_unique<vr::WebXrSharedBuffer>();
  // Local resources
  glGenTextures(1, &buffer->local_texture);
  return buffer;
}

gpu::MailboxHolder ArImageTransport::TransferFrame(
    const gfx::Size& frame_size,
    const gfx::Transform& uv_transform) {
  DCHECK(IsOnGlThread());

  if (!webxr_->GetAnimatingFrame()->shared_buffer) {
    webxr_->GetAnimatingFrame()->shared_buffer = CreateBuffer();
  }
  vr::WebXrSharedBuffer* shared_buffer =
      webxr_->GetAnimatingFrame()->shared_buffer.get();
  ResizeSharedBuffer(frame_size, shared_buffer);

  mailbox_bridge_->GenSyncToken(&shared_buffer->mailbox_holder.sync_token);
  return shared_buffer->mailbox_holder;
}

void ArImageTransport::CreateGpuFenceForSyncToken(
    const gpu::SyncToken& sync_token,
    base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) {
  DVLOG(2) << __func__;
  mailbox_bridge_->CreateGpuFence(sync_token, std::move(callback));
}

void ArImageTransport::WaitSyncToken(const gpu::SyncToken& sync_token) {
  mailbox_bridge_->WaitSyncToken(sync_token);
}

void ArImageTransport::CopyCameraImageToFramebuffer(
    const gfx::Size& frame_size,
    const gfx::Transform& uv_transform) {
  glDisable(GL_BLEND);
  CopyTextureToFramebuffer(camera_texture_id_arcore_, frame_size, uv_transform);
}

void ArImageTransport::CopyDrawnImageToFramebuffer(
    const gfx::Size& frame_size,
    const gfx::Transform& uv_transform) {
  DVLOG(2) << __func__;

  vr::WebXrSharedBuffer* shared_buffer =
      webxr_->GetRenderingFrame()->shared_buffer.get();

  // Set the blend mode for combining the drawn image (source) with the camera
  // image (destination). WebXR assumes that the canvas has premultiplied alpha,
  // so the source blend function is GL_ONE. The destination blend function is
  // (1 - src_alpha) as usual. (Setting that to GL_ONE would simulate an
  // additive AR headset that can't draw opaque black.)
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
  CopyTextureToFramebuffer(shared_buffer->local_texture, frame_size,
                           uv_transform);
}

void ArImageTransport::CopyTextureToFramebuffer(
    GLuint texture,
    const gfx::Size& frame_size,
    const gfx::Transform& uv_transform) {
  DVLOG(2) << __func__;
  // Don't need face culling, depth testing, blending, etc. Turn it all off.
  // TODO(klausw): see if we can do this one time on initialization. That would
  // be a tiny bit more efficient, but is only safe if ARCore and ArRenderer
  // don't modify these states.
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glViewport(0, 0, frame_size.width(), frame_size.height());

  // Draw the ARCore texture!
  float uv_transform_floats[16];
  uv_transform.matrix().asColMajorf(uv_transform_floats);
  ar_renderer_->Draw(texture, uv_transform_floats, 0, 0);
}

bool ArImageTransport::IsOnGlThread() const {
  return gl_thread_task_runner_->BelongsToCurrentThread();
}

std::unique_ptr<ArImageTransport> ArImageTransportFactory::Create(
    std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge) {
  return std::make_unique<ArImageTransport>(std::move(mailbox_bridge));
}

}  // namespace device
