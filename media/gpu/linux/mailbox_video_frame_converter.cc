// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/linux/mailbox_video_frame_converter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "media/base/format_utils.h"
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

// static
std::unique_ptr<VideoFrameConverter> MailboxVideoFrameConverter::Create(
    UnwrapFrameCB unwrap_frame_cb,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetCommandBufferStubCB get_stub_cb) {
  if (!unwrap_frame_cb || !gpu_task_runner || !get_stub_cb)
    return nullptr;

  return base::WrapUnique<VideoFrameConverter>(new MailboxVideoFrameConverter(
      std::move(unwrap_frame_cb), std::move(gpu_task_runner),
      std::move(get_stub_cb)));
}

MailboxVideoFrameConverter::MailboxVideoFrameConverter(
    UnwrapFrameCB unwrap_frame_cb,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetCommandBufferStubCB get_stub_cb)
    : unwrap_frame_cb_(std::move(unwrap_frame_cb)),
      gpu_task_runner_(std::move(gpu_task_runner)),
      get_stub_cb_(std::move(get_stub_cb)) {
  DVLOGF(2);

  parent_weak_this_ = parent_weak_this_factory_.GetWeakPtr();
  gpu_weak_this_ = gpu_weak_this_factory_.GetWeakPtr();
}

void MailboxVideoFrameConverter::Destroy() {
  DCHECK(!parent_task_runner_ ||
         parent_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(2);

  parent_weak_this_factory_.InvalidateWeakPtrs();
  gpu_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MailboxVideoFrameConverter::DestroyOnGPUThread,
                                gpu_weak_this_));
}

void MailboxVideoFrameConverter::DestroyOnGPUThread() {
  DCHECK(gpu_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(2);

  gpu_weak_this_factory_.InvalidateWeakPtrs();
  delete this;
}

MailboxVideoFrameConverter::~MailboxVideoFrameConverter() {
  // |gpu_weak_this_factory_| is already invalidated here.
  DCHECK(gpu_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(2);
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

void MailboxVideoFrameConverter::ConvertFrame(scoped_refptr<VideoFrame> frame) {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(4);

  if (!frame || !frame->HasDmaBufs())
    return OnError("Invalid frame.");

  VideoFrame* origin_frame = unwrap_frame_cb_.Run(*frame);
  if (!origin_frame)
    return OnError("Failed to get origin frame.");

  // Generate mailbox at gpu thread if we haven't.
  const int origin_frame_id = origin_frame->unique_id();
  if (mailbox_table_.find(origin_frame_id) == mailbox_table_.end()) {
    DVLOGF(4) << "Generate mailbox for frame: " << origin_frame_id;
    // Set an empty mailbox first to prevent generating mailbox multiple times.
    mailbox_table_.emplace(origin_frame_id, gpu::Mailbox());

    // |frame| keeps a refptr of |origin_frame|. |origin_frame| is guaranteed
    // alive by carrying |frame|. So it's safe to use base::Unretained here.
    gpu_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MailboxVideoFrameConverter::GenerateMailbox,
                       gpu_weak_this_, base::Unretained(origin_frame), frame));
  }

  input_frame_queue_.emplace(std::move(frame), origin_frame_id);
  TryOutputFrames();
}

void MailboxVideoFrameConverter::TryOutputFrames() {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(4) << "input_frame_queue_ size: " << input_frame_queue_.size();

  while (!input_frame_queue_.empty()) {
    const int origin_frame_id = input_frame_queue_.front().second;
    const gpu::Mailbox& mailbox = mailbox_table_[origin_frame_id];
    if (mailbox.IsZero()) {
      DVLOGF(4) << "Mailbox for frame: " << origin_frame_id
                << " is not generated yet.";
      return;
    }

    auto frame = std::move(input_frame_queue_.front().first);
    input_frame_queue_.pop();

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

    mailbox_frame->metadata()->SetBoolean(
        VideoFrameMetadata::READ_LOCK_FENCES_ENABLED, true);

    output_cb_.Run(mailbox_frame);
  }
}

void MailboxVideoFrameConverter::GenerateMailbox(
    VideoFrame* origin_frame,
    scoped_refptr<VideoFrame> frame) {
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DVLOGF(4) << "frame: " << origin_frame->unique_id();

  // CreateCommandBufferHelper() should be called on |gpu_task_runner_| so we
  // call it here lazily instead of at constructor.
  if (!command_buffer_helper_ && !CreateCommandBufferHelper())
    return OnError("Failed to create command buffer helper.");

  // Get NativePixmap.
  scoped_refptr<gfx::NativePixmap> pixmap;
  auto buffer_format =
      VideoPixelFormatToGfxBufferFormat(origin_frame->format());
  if (!buffer_format) {
    return OnError("Unsupported format: " +
                   VideoPixelFormatToString(origin_frame->format()));
  }

#if defined(USE_OZONE)
  gfx::GpuMemoryBufferHandle handle = CreateGpuMemoryBufferHandle(origin_frame);
  DCHECK(!handle.is_null());
  pixmap = ui::OzonePlatform::GetInstance()
               ->GetSurfaceFactoryOzone()
               ->CreateNativePixmapFromHandle(
                   gfx::kNullAcceleratedWidget, origin_frame->coded_size(),
                   *buffer_format, std::move(handle.native_pixmap_handle));
#endif  // defined(USE_OZONE)
  if (!pixmap)
    return OnError("Cannot create NativePixmap.");

  // Create GLImage.
  auto image = base::MakeRefCounted<gl::GLImageNativePixmap>(
      origin_frame->coded_size(), *buffer_format);
  if (!image->Initialize(std::move(pixmap)))
    return OnError("Failed to initialize GLImage.");

  // Create texture and bind image to texture.
  if (!command_buffer_helper_->MakeContextCurrent())
    return OnError("Failed to make context current.");

  GLuint service_id = command_buffer_helper_->CreateTexture(
      kTextureTarget, GL_RGBA, origin_frame->coded_size().width(),
      origin_frame->coded_size().height(), GL_RGBA, GL_UNSIGNED_BYTE);
  DCHECK(service_id);
  gl::ScopedTextureBinder bind_restore(kTextureTarget, service_id);
  bool ret = image->BindTexImage(kTextureTarget);
  DCHECK(ret);
  command_buffer_helper_->BindImage(service_id, image.get(), true);
  command_buffer_helper_->SetCleared(service_id);
  gpu::Mailbox mailbox = command_buffer_helper_->CreateMailbox(service_id);

  // Destroy the texture after the DMA-buf VideoFrame is destructed.
  origin_frame->AddDestructionObserver(base::BindOnce(
      &DestroyTexture, gpu_task_runner_, command_buffer_helper_, service_id));

  // |frame| keeps a refptr of |origin_frame|. |origin_frame| is guaranteed
  // alive by carrying |frame|. So it's safe to use base::Unretained here.
  parent_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MailboxVideoFrameConverter::RegisterMailbox,
                     parent_weak_this_, base::Unretained(origin_frame), mailbox,
                     std::move(frame)));
}

void MailboxVideoFrameConverter::RegisterMailbox(
    VideoFrame* origin_frame,
    const gpu::Mailbox& mailbox,
    scoped_refptr<VideoFrame> frame) {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!mailbox.IsZero());
  DVLOGF(4) << "frame: " << origin_frame->unique_id();

  mailbox_table_[origin_frame->unique_id()] = mailbox;
  origin_frame->AddDestructionObserver(base::BindOnce(
      &MailboxVideoFrameConverter::UnregisterMailboxThunk, parent_task_runner_,
      parent_weak_this_, origin_frame->unique_id()));

  // Mailbox has been generated. It's time to convert frame again.
  TryOutputFrames();
}

// static
void MailboxVideoFrameConverter::UnregisterMailboxThunk(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::Optional<base::WeakPtr<MailboxVideoFrameConverter>> converter,
    const int origin_frame_id) {
  DCHECK(converter);
  DVLOGF(4) << "frame: " << origin_frame_id;

  // MailboxVideoFrameConverter might have already been destroyed when this
  // method is called. In this case, the WeakPtr will have been invalidated at
  // |parent_task_runner_|, and UnregisterMailbox() will not get executed.
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&MailboxVideoFrameConverter::UnregisterMailbox,
                                *converter, origin_frame_id));
}

void MailboxVideoFrameConverter::UnregisterMailbox(const int origin_frame_id) {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(4);

  auto it = mailbox_table_.find(origin_frame_id);
  DCHECK(it != mailbox_table_.end());
  mailbox_table_.erase(it);
}

void MailboxVideoFrameConverter::AbortPendingFrames() {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(4) << "Number of pending frames: " << input_frame_queue_.size();

  input_frame_queue_ = {};
}

bool MailboxVideoFrameConverter::HasPendingFrames() const {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(4) << "Number of pending frames: " << input_frame_queue_.size();

  return !input_frame_queue_.empty();
}

void MailboxVideoFrameConverter::OnError(const std::string& msg) {
  VLOGF(1) << msg;

  parent_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MailboxVideoFrameConverter::AbortPendingFrames,
                                parent_weak_this_));
  // Currently we don't have a dedicated callback to notify client that error
  // occurs. Output a null frame to indicate any error occurs.
  // TODO(akahuang): Create an error notification callback.
  parent_task_runner_->PostTask(FROM_HERE, base::BindOnce(output_cb_, nullptr));
}

}  // namespace media
