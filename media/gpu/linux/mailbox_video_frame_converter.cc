// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/linux/mailbox_video_frame_converter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "media/gpu/format_utils.h"
#include "media/gpu/linux/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/gl/scoped_binders.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif  // defined(USE_OZONE)

namespace media {

namespace {

constexpr GLenum kTextureTarget = GL_TEXTURE_EXTERNAL_OES;

// Destroy the GL texture. This is called when the origin DMA-buf VideoFrame
// is destroyed.
void DestroyTexture(scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
                    scoped_refptr<CommandBufferHelper> command_buffer_helper,
                    GLuint service_id) {
  DVLOGF(4);

  if (!gpu_task_runner->BelongsToCurrentThread()) {
    gpu_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&DestroyTexture, std::move(gpu_task_runner),
                       std::move(command_buffer_helper), service_id));
    return;
  }

  if (!command_buffer_helper->MakeContextCurrent()) {
    VLOGF(1) << "Failed to make context current";
    return;
  }
  command_buffer_helper->DestroyTexture(service_id);
}

// ReleaseMailbox callback of the mailbox VideoFrame.
// Keep the wrapped DMA-buf VideoFrame until WaitForSyncToken() is done.
void WaitForSyncToken(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    scoped_refptr<CommandBufferHelper> command_buffer_helper,
    scoped_refptr<VideoFrame> frame,
    const gpu::SyncToken& sync_token) {
  DVLOGF(4);

  gpu_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CommandBufferHelper::WaitForSyncToken,
          std::move(command_buffer_helper), sync_token,
          base::BindOnce(base::DoNothing::Once<scoped_refptr<VideoFrame>>(),
                         std::move(frame))));
}

}  // namespace

MailboxVideoFrameConverter::MailboxVideoFrameConverter(
    UnwrapFrameCB unwrap_frame_cb,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetCommandBufferStubCB get_stub_cb)
    : unwrap_frame_cb_(std::move(unwrap_frame_cb)),
      gpu_task_runner_(std::move(gpu_task_runner)),
      get_stub_cb_(std::move(get_stub_cb)),
      weak_this_factory_(this) {
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

MailboxVideoFrameConverter::~MailboxVideoFrameConverter() {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());

  weak_this_factory_.InvalidateWeakPtrs();
}

bool MailboxVideoFrameConverter::CreateCommandBufferHelper() {
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK(get_stub_cb_);
  DVLOGF(4);

  gpu::CommandBufferStub* stub = std::move(get_stub_cb_).Run();
  if (!stub) {
    VLOGF(1) << "Failed to obtain command buffer stub";
    return false;
  }

  command_buffer_helper_ = CommandBufferHelper::Create(stub);
  return command_buffer_helper_ != nullptr;
}

scoped_refptr<VideoFrame> MailboxVideoFrameConverter::ConvertFrame(
    scoped_refptr<VideoFrame> frame) {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(4);

  if (!frame) {
    DVLOGF(1) << "nullptr input.";
    return nullptr;
  }
  if (!frame->HasDmaBufs()) {
    DVLOGF(1) << "Only converting DMA-buf frames is supported.";
    return nullptr;
  }

  VideoFrame* origin_frame = unwrap_frame_cb_.Run(*frame);
  gpu::Mailbox mailbox;
  auto it = mailbox_table_.find(origin_frame->unique_id());
  if (it != mailbox_table_.end())
    mailbox = it->second;

  if (mailbox.IsZero()) {
    base::WaitableEvent event;
    // We wait until GenerateMailbox() finished, so base::Unretained(this) is
    // safe.
    gpu_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MailboxVideoFrameConverter::GenerateMailbox,
                       base::Unretained(this), base::Unretained(origin_frame),
                       base::Unretained(&mailbox), base::Unretained(&event)));
    event.Wait();

    if (mailbox.IsZero()) {
      VLOGF(1) << "Failed to create mailbox.";
      return nullptr;
    }

    RegisterMailbox(origin_frame, mailbox);
  }

  gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
  mailbox_holders[0] =
      gpu::MailboxHolder(mailbox, gpu::SyncToken(), kTextureTarget);
  scoped_refptr<VideoFrame> mailbox_frame = VideoFrame::WrapNativeTextures(
      frame->format(), mailbox_holders,
      base::BindOnce(&WaitForSyncToken, gpu_task_runner_,
                     command_buffer_helper_, frame),
      frame->coded_size(), frame->visible_rect(), frame->natural_size(),
      frame->timestamp());
  mailbox_frame->metadata()->MergeMetadataFrom(frame->metadata());
  return mailbox_frame;
}

void MailboxVideoFrameConverter::GenerateMailbox(VideoFrame* origin_frame,
                                                 gpu::Mailbox* mailbox,
                                                 base::WaitableEvent* event) {
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DVLOGF(4);

  // Signal the event when leaving the method.
  base::ScopedClosureRunner signal_event(
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(event)));

  // CreateCommandBufferHelper() should be called on |gpu_task_runner_| so we
  // call it here lazily instead of at constructor.
  if (!command_buffer_helper_ && !CreateCommandBufferHelper()) {
    VLOGF(1) << "Failed to create command buffer helper.";
    return;
  }

  // Get NativePixmap.
  scoped_refptr<gfx::NativePixmap> pixmap;
  auto buffer_format =
      VideoPixelFormatToGfxBufferFormat(origin_frame->format());
  if (!buffer_format) {
    VLOGF(1) << "Unsupported format: " << origin_frame->format();
    return;
  };
#if defined(USE_OZONE)
  gfx::GpuMemoryBufferHandle handle = CreateGpuMemoryBufferHandle(origin_frame);
  DCHECK(!handle.is_null());
  pixmap = ui::OzonePlatform::GetInstance()
               ->GetSurfaceFactoryOzone()
               ->CreateNativePixmapFromHandle(
                   gfx::kNullAcceleratedWidget, origin_frame->coded_size(),
                   *buffer_format, std::move(handle.native_pixmap_handle));
#endif  // defined(USE_OZONE)
  if (!pixmap) {
    VLOGF(1) << "Cannot create NativePixmap.";
    return;
  }

  // Create GLImage.
  auto image = base::MakeRefCounted<gl::GLImageNativePixmap>(
      origin_frame->coded_size(), *buffer_format);
  if (!image->Initialize(std::move(pixmap))) {
    VLOGF(1) << "Failed to initialize GLImage.";
    return;
  }

  // Create texture and bind image to texture.
  if (!command_buffer_helper_->MakeContextCurrent()) {
    VLOGF(1) << "Failed to make context current.";
    return;
  }
  GLuint service_id = command_buffer_helper_->CreateTexture(
      kTextureTarget, GL_RGBA, origin_frame->coded_size().width(),
      origin_frame->coded_size().height(), GL_RGBA, GL_UNSIGNED_BYTE);
  DCHECK(service_id);
  gl::ScopedTextureBinder bind_restore(kTextureTarget, service_id);
  bool ret = image->BindTexImage(kTextureTarget);
  DCHECK(ret);
  command_buffer_helper_->BindImage(service_id, image.get(), true);
  command_buffer_helper_->SetCleared(service_id);
  *mailbox = command_buffer_helper_->CreateMailbox(service_id);

  // Destroy the texture after the DMA-buf VideoFrame is destructed.
  origin_frame->AddDestructionObserver(base::BindOnce(
      &DestroyTexture, gpu_task_runner_, command_buffer_helper_, service_id));
  return;
}

void MailboxVideoFrameConverter::RegisterMailbox(VideoFrame* origin_frame,
                                                 const gpu::Mailbox& mailbox) {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!mailbox.IsZero());
  DVLOGF(4);

  auto ret =
      mailbox_table_.insert(std::make_pair(origin_frame->unique_id(), mailbox));
  DCHECK(ret.second);
  origin_frame->AddDestructionObserver(base::BindOnce(
      &MailboxVideoFrameConverter::UnregisterMailboxThunk, parent_task_runner_,
      weak_this_, origin_frame->unique_id()));
}

// static
void MailboxVideoFrameConverter::UnregisterMailboxThunk(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::Optional<base::WeakPtr<MailboxVideoFrameConverter>> converter,
    int origin_frame_id) {
  DCHECK(converter);
  DVLOGF(4);

  // MailboxVideoFrameConverter might have already been destroyed when this
  // method is called. In this case, the WeakPtr will have been invalidated at
  // |parent_task_runner_|, and UnregisterMailbox() will not get executed.
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&MailboxVideoFrameConverter::UnregisterMailbox,
                                *converter, origin_frame_id));
}

void MailboxVideoFrameConverter::UnregisterMailbox(int origin_frame_id) {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(4);

  auto it = mailbox_table_.find(origin_frame_id);
  DCHECK(it != mailbox_table_.end());
  mailbox_table_.erase(it);
}

}  // namespace media
