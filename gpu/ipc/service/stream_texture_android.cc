// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/stream_texture_android.h"

#include <string.h>

#include "base/bind.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/abstract_texture_impl_shared_context_state.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/ipc/common/android/scoped_surface_request_conduit.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {
namespace {
std::unique_ptr<ui::ScopedMakeCurrent> MakeCurrent(
    SharedContextState* context_state) {
  std::unique_ptr<ui::ScopedMakeCurrent> scoped_make_current;
  bool needs_make_current = !context_state->IsCurrent(nullptr);
  if (needs_make_current) {
    scoped_make_current = std::make_unique<ui::ScopedMakeCurrent>(
        context_state->context(), context_state->surface());
  }
  return scoped_make_current;
}

class SharedImageBackingStreamTexture : public gpu::SharedImageBacking {
 public:
  SharedImageBackingStreamTexture(
      const gpu::Mailbox& mailbox,
      const gfx::Size& size,
      std::unique_ptr<gpu::gles2::AbstractTexture> abstract_texture)
      : SharedImageBacking(
            mailbox,
            viz::RGBA_8888,
            size,
            gfx::ColorSpace(),
            gpu::SHARED_IMAGE_USAGE_DISPLAY | gpu::SHARED_IMAGE_USAGE_GLES2,
            viz::ResourceSizes::UncheckedSizeInBytes<size_t>(size,
                                                             viz::RGBA_8888),
            false /* is_thread_safe */),
        abstract_texture_(std::move(abstract_texture)) {}

  ~SharedImageBackingStreamTexture() override {}

  // SharedImageBacking implementation.
  bool IsCleared() const override { return true; }
  void SetCleared() override {}
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override {
    NOTREACHED();
  }
  bool ProduceLegacyMailbox(gpu::MailboxManager* mailbox_manager) override {
    mailbox_manager->ProduceTexture(mailbox(),
                                    abstract_texture_->GetTextureBase());
    return true;
  }
  void Destroy() override {}

 private:
  // |abstract_texture_| is only used for legacy mailbox.
  std::unique_ptr<gpu::gles2::AbstractTexture> abstract_texture_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageBackingStreamTexture);
};

}  // namespace

// static
scoped_refptr<StreamTexture> StreamTexture::Create(GpuChannel* channel,
                                                   int stream_id) {
  ContextResult result;
  auto context_state =
      channel->gpu_channel_manager()->GetSharedContextState(&result);
  if (result != ContextResult::kSuccess)
    return nullptr;
  auto scoped_make_current = MakeCurrent(context_state.get());
  auto texture = std::make_unique<gles2::AbstractTextureImplOnSharedContext>(
      GL_TEXTURE_EXTERNAL_OES, GL_RGBA, 0, 0, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
      context_state.get());
  return new StreamTexture(channel, stream_id, std::move(texture),
                           std::move(context_state));
}

StreamTexture::StreamTexture(
    GpuChannel* channel,
    int32_t route_id,
    std::unique_ptr<gles2::AbstractTexture> surface_owner_texture,
    scoped_refptr<SharedContextState> context_state)
    : surface_owner_texture_(std::move(surface_owner_texture)),
      surface_owner_texture_id_(
          surface_owner_texture_->GetTextureBase()->service_id()),
      surface_owner_(SurfaceOwner::Create(surface_owner_texture_id_)),
      size_(0, 0),
      has_pending_frame_(false),
      channel_(channel),
      route_id_(route_id),
      has_listener_(false),
      context_state_(std::move(context_state)),
      sequence_(
          channel_->scheduler()->CreateSequence(SchedulingPriority::kLow)),
      sync_point_client_state_(
          channel_->sync_point_manager()->CreateSyncPointClientState(
              CommandBufferNamespace::GPU_IO,
              CommandBufferIdFromChannelAndRoute(channel_->client_id(),
                                                 route_id),
              sequence_)),
      weak_factory_(this) {
  context_state_->AddContextLostObserver(this);
  memset(current_matrix_, 0, sizeof(current_matrix_));
  channel->AddRoute(route_id, sequence_, this);
  surface_owner_->SetFrameAvailableCallback(base::BindRepeating(
      &StreamTexture::OnFrameAvailable, weak_factory_.GetWeakPtr()));
}

StreamTexture::~StreamTexture() {
  // |channel_| is always released before GpuChannel releases its reference to
  // this class.
  DCHECK(!channel_);
  context_state_->RemoveContextLostObserver(this);
}

void StreamTexture::ReleaseChannel() {
  DCHECK(channel_);
  channel_->RemoveRoute(route_id_);
  channel_->scheduler()->DestroySequence(sequence_);
  sequence_ = SequenceId();
  sync_point_client_state_->Destroy();
  sync_point_client_state_ = nullptr;
  channel_ = nullptr;
}

// gpu::gles2::GLStreamTextureMatrix implementation
void StreamTexture::GetTextureMatrix(float xform[16]) {
  if (surface_owner_) {
    UpdateTexImage();
    surface_owner_->GetTransformMatrix(current_matrix_);
  }
  memcpy(xform, current_matrix_, sizeof(current_matrix_));
  YInvertMatrix(xform);
}

void StreamTexture::OnContextLost() {
  surface_owner_ = nullptr;
}

void StreamTexture::UpdateTexImage() {
  DCHECK(surface_owner_.get());

  if (!has_pending_frame_) return;

  auto scoped_make_current = MakeCurrent(context_state_.get());
  gl::ScopedTextureBinder scoped_bind_texture(GL_TEXTURE_EXTERNAL_OES,
                                              surface_owner_texture_id_);
  surface_owner_->UpdateTexImage();
  has_pending_frame_ = false;
}

bool StreamTexture::CopyTexImage(unsigned target) {
  if (target != GL_TEXTURE_EXTERNAL_OES)
    return false;

  if (!surface_owner_.get())
    return false;

  GLint texture_id;
  glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &texture_id);

  // The following code only works if we're being asked to copy into
  // |texture_id_|. Copying into a different texture is not supported.
  // On some devices GL_TEXTURE_BINDING_EXTERNAL_OES is not supported as
  // glGetIntegerv() parameter. In this case the value of |texture_id| will be
  // zero and we assume that it is properly bound to |texture_id_|.
  if (texture_id > 0 &&
      static_cast<unsigned>(texture_id) != surface_owner_texture_id_)
    return false;

  UpdateTexImage();

  return true;
}

void StreamTexture::OnFrameAvailable() {
  has_pending_frame_ = true;
  if (has_listener_ && channel_) {
    channel_->Send(new GpuStreamTextureMsg_FrameAvailable(route_id_));
  }
}

gfx::Size StreamTexture::GetSize() {
  return size_;
}

unsigned StreamTexture::GetInternalFormat() {
  return GL_RGBA;
}

bool StreamTexture::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(StreamTexture, message)
    IPC_MESSAGE_HANDLER(GpuStreamTextureMsg_StartListening, OnStartListening)
    IPC_MESSAGE_HANDLER(GpuStreamTextureMsg_ForwardForSurfaceRequest,
                        OnForwardForSurfaceRequest)
    IPC_MESSAGE_HANDLER(GpuStreamTextureMsg_CreateSharedImage,
                        OnCreateSharedImage)
    IPC_MESSAGE_HANDLER(GpuStreamTextureMsg_Destroy, OnDestroy)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  DCHECK(handled);
  return handled;
}

void StreamTexture::OnStartListening() {
  DCHECK(!has_listener_);
  has_listener_ = true;
}

void StreamTexture::OnForwardForSurfaceRequest(
    const base::UnguessableToken& request_token) {
  if (!channel_)
    return;

  ScopedSurfaceRequestConduit::GetInstance()
      ->ForwardSurfaceOwnerForSurfaceRequest(request_token,
                                             surface_owner_.get());
}

void StreamTexture::OnCreateSharedImage(const gpu::Mailbox& mailbox,
                                        const gfx::Size& size,
                                        uint32_t release_id) {
  DCHECK(channel_);
  size_ = size;

  if (!surface_owner_)
    return;

  // We do not update |surface_owner_texture_|'s internal gles2::Texture's
  // size. This is because the gles2::Texture is never used directly, the
  // associated |surface_owner_texture_id_| being the only part of that object
  // we interact with.
  // If we ever use |surface_owner_texture_|, we need to ensure that it gets
  // updated here.

  auto scoped_make_current = MakeCurrent(context_state_.get());
  auto legacy_mailbox_texture =
      std::make_unique<gles2::AbstractTextureImplOnSharedContext>(
          GL_TEXTURE_EXTERNAL_OES, GL_RGBA, size.width(), size.height(), 1, 0,
          GL_RGBA, GL_UNSIGNED_BYTE, context_state_.get());
  legacy_mailbox_texture->BindStreamTextureImage(this,
                                                 surface_owner_texture_id_);

  auto shared_image = std::make_unique<SharedImageBackingStreamTexture>(
      mailbox, size_, std::move(legacy_mailbox_texture));
  channel_->shared_image_stub()->factory()->RegisterBacking(
      std::move(shared_image), true /* allow_legacy_mailbox */);

  SyncToken sync_token(sync_point_client_state_->namespace_id(),
                       sync_point_client_state_->command_buffer_id(),
                       release_id);
  auto* mailbox_manager = channel_->gpu_channel_manager()->mailbox_manager();
  mailbox_manager->PushTextureUpdates(sync_token);
  sync_point_client_state_->ReleaseFenceSync(release_id);
}

void StreamTexture::OnDestroy() {
  DCHECK(channel_);

  // The following call may delete the StreamTexture, so we must ensure that no
  // access to |this| occurs after the call.
  channel_->DestroyStreamTexture(route_id_);
}

StreamTexture::BindOrCopy StreamTexture::ShouldBindOrCopy() {
  return COPY;
}

bool StreamTexture::BindTexImage(unsigned target) {
  NOTREACHED();
  return false;
}

void StreamTexture::ReleaseTexImage(unsigned target) {
}

bool StreamTexture::CopyTexSubImage(unsigned target,
                                    const gfx::Point& offset,
                                    const gfx::Rect& rect) {
  return false;
}

bool StreamTexture::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int z_order,
    gfx::OverlayTransform transform,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  NOTREACHED();
  return false;
}

void StreamTexture::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                                 uint64_t process_tracing_id,
                                 const std::string& dump_name) {
  // TODO(ericrk): Add OnMemoryDump for GLImages. crbug.com/514914
}

bool StreamTexture::HasMutableState() const {
  return false;
}

}  // namespace gpu
