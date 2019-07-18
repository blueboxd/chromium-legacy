// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/video_frame_factory_impl.h"

#include <memory>

#include "base/android/android_image_reader_compat.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/gpu/android/codec_image.h"
#include "media/gpu/android/codec_image_group.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/android/maybe_render_early_manager.h"
#include "media/gpu/android/shared_image_video.h"
#include "media/gpu/command_buffer_helper.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/gl/scoped_make_current.h"

namespace media {
namespace {

TextureOwner::Mode GetTextureOwnerMode(
    VideoFrameFactory::OverlayMode overlay_mode) {
  const bool a_image_reader_supported =
      base::android::AndroidImageReader::GetInstance().IsSupported();

  switch (overlay_mode) {
    case VideoFrameFactory::OverlayMode::kDontRequestPromotionHints:
    case VideoFrameFactory::OverlayMode::kRequestPromotionHints:
      return a_image_reader_supported && base::FeatureList::IsEnabled(
                                             media::kAImageReaderVideoOutput)
                 ? TextureOwner::Mode::kAImageReaderInsecure
                 : TextureOwner::Mode::kSurfaceTextureInsecure;
    case VideoFrameFactory::OverlayMode::kSurfaceControlSecure:
      DCHECK(a_image_reader_supported);
      return TextureOwner::Mode::kAImageReaderSecureSurfaceControl;
    case VideoFrameFactory::OverlayMode::kSurfaceControlInsecure:
      DCHECK(a_image_reader_supported);
      return TextureOwner::Mode::kAImageReaderInsecureSurfaceControl;
  }

  NOTREACHED();
  return TextureOwner::Mode::kSurfaceTextureInsecure;
}

// Run on the GPU main thread to allocate the texture owner, and return it
// via |init_cb|.
static void AllocateTextureOwnerOnGpuThread(
    VideoFrameFactory::InitCb init_cb,
    VideoFrameFactory::OverlayMode overlay_mode,
    scoped_refptr<gpu::SharedContextState> shared_context_state) {
  if (!shared_context_state) {
    std::move(init_cb).Run(nullptr);
    return;
  }

  std::move(init_cb).Run(
      TextureOwner::Create(TextureOwner::CreateTexture(shared_context_state),
                           GetTextureOwnerMode(overlay_mode)));
}

}  // namespace

using gpu::gles2::AbstractTexture;

VideoFrameFactoryImpl::VideoFrameFactoryImpl(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    const gpu::GpuPreferences& gpu_preferences,
    std::unique_ptr<SharedImageVideoProvider> image_provider,
    std::unique_ptr<MaybeRenderEarlyManager> mre_manager)
    : image_provider_(std::move(image_provider)),
      gpu_task_runner_(std::move(gpu_task_runner)),
      enable_threaded_texture_mailboxes_(
          gpu_preferences.enable_threaded_texture_mailboxes),
      mre_manager_(std::move(mre_manager)),
      weak_factory_(this) {}

VideoFrameFactoryImpl::~VideoFrameFactoryImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VideoFrameFactoryImpl::Initialize(OverlayMode overlay_mode,
                                       InitCb init_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  overlay_mode_ = overlay_mode;
  // On init success, create the TextureOwner and hop it back to this thread to
  // call |init_cb|.
  auto gpu_init_cb =
      base::BindOnce(&AllocateTextureOwnerOnGpuThread,
                     BindToCurrentLoop(std::move(init_cb)), overlay_mode);
  image_provider_->Initialize(std::move(gpu_init_cb));
}

void VideoFrameFactoryImpl::SetSurfaceBundle(
    scoped_refptr<CodecSurfaceBundle> surface_bundle) {
  scoped_refptr<CodecImageGroup> image_group;
  if (!surface_bundle) {
    // Clear everything, just so we're not holding a reference.
    texture_owner_ = nullptr;
  } else {
    // If |surface_bundle| is using a TextureOwner, then get it.  Note that the
    // only reason we need this is for legacy mailbox support; we send it to
    // the SharedImageVideoProvider so that (eventually) it can get the service
    // id from the owner for the legacy mailbox texture.  Otherwise, this would
    // be a lot simpler.
    texture_owner_ =
        surface_bundle->overlay() ? nullptr : surface_bundle->texture_owner();

    // TODO(liberato): When we enable pooling, do we need to clear the pool
    // here because the CodecImageGroup has changed?  It's unclear, since the
    // CodecImage shouldn't be in any group once we re-use it, so maybe it's
    // fine to take no action.

    mre_manager_->SetSurfaceBundle(std::move(surface_bundle));
  }
}

void VideoFrameFactoryImpl::CreateVideoFrame(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    base::TimeDelta timestamp,
    gfx::Size natural_size,
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
    OnceOutputCb output_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  gfx::Size coded_size = output_buffer->size();
  gfx::Rect visible_rect(coded_size);
  // The pixel format doesn't matter as long as it's valid for texture frames.
  VideoPixelFormat pixel_format = PIXEL_FORMAT_ARGB;

  // Check that we can create a VideoFrame for this config before trying to
  // create the textures for it.
  if (!VideoFrame::IsValidConfig(pixel_format, VideoFrame::STORAGE_OPAQUE,
                                 coded_size, visible_rect, natural_size)) {
    LOG(ERROR) << __func__ << " unsupported video frame format";
    std::move(output_cb).Run(nullptr);
    return;
  }

  SharedImageVideoProvider::ImageSpec spec(coded_size);

  auto image_ready_cb = base::BindOnce(
      &VideoFrameFactoryImpl::OnImageReady, weak_factory_.GetWeakPtr(),
      std::move(output_cb), timestamp, coded_size, natural_size,
      std::move(output_buffer), texture_owner_, std::move(promotion_hint_cb),
      pixel_format, overlay_mode_, enable_threaded_texture_mailboxes_,
      gpu_task_runner_);

  image_provider_->RequestImage(std::move(image_ready_cb), spec,
                                texture_owner_);
}

// static
void VideoFrameFactoryImpl::OnImageReady(
    base::WeakPtr<VideoFrameFactoryImpl> thiz,
    OnceOutputCb output_cb,
    base::TimeDelta timestamp,
    gfx::Size coded_size,
    gfx::Size natural_size,
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    scoped_refptr<TextureOwner> texture_owner,
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
    VideoPixelFormat pixel_format,
    OverlayMode overlay_mode,
    bool enable_threaded_texture_mailboxes,
    scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
    SharedImageVideoProvider::ImageRecord record) {
  TRACE_EVENT0("media", "VideoVideoFrameFactoryImpl::OnVideoFrameImageReady");

  if (!thiz)
    return;

  // Initialize the CodecImage to use this output buffer.  Note that we're not
  // on the gpu main thread here, but it's okay since CodecImage is not being
  // used at this point.  Alternatively, we could post it, or hand it off to the
  // MaybeRenderEarlyManager to save a post.
  record.codec_image_holder->codec_image_raw()->Initialize(
      std::move(output_buffer), texture_owner, std::move(promotion_hint_cb));

  // Send the CodecImage (via holder, since we can't touch the refcount here) to
  // the MaybeRenderEarlyManager.
  thiz->mre_manager()->AddCodecImage(std::move(record.codec_image_holder));

  gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
  mailbox_holders[0] = gpu::MailboxHolder(record.mailbox, gpu::SyncToken(),
                                          GL_TEXTURE_EXTERNAL_OES);

  // TODO(liberato): We should set the promotion hint cb here on the image.  We
  // should also set the output buffer params; we shouldn't send the output
  // buffer to the gpu thread, since the codec image isn't in use anyway.  We
  // can access it on any thread.  We'll also need to get new images when we
  // switch texture owners.  That's left for future work.

  // TODO(liberato): When we switch to a pool, we need to provide some way to
  // call MaybeRenderEarly that doesn't depend on |release_cb|.  I suppose we
  // could get a RepeatingCallback that's a "reuse cb", that we'd attach to the
  // VideoFrame's release cb, since we have to wait for the sync token anyway.
  // That would run on the gpu thread, and could MaybeRenderEarly.

  gfx::Rect visible_rect(coded_size);

  auto frame = VideoFrame::WrapNativeTextures(
      pixel_format, mailbox_holders, VideoFrame::ReleaseMailboxCB(), coded_size,
      visible_rect, natural_size, timestamp);

  frame->set_ycbcr_info(record.ycbcr_info);
  // If, for some reason, we failed to create a frame, then fail.  Note that we
  // don't need to call |release_cb|; dropping it is okay since the api says so.
  if (!frame) {
    LOG(ERROR) << __func__ << " failed to create video frame";
    std::move(output_cb).Run(nullptr);
    return;
  }

  // The frames must be copied when threaded texture mailboxes are in use
  // (http://crbug.com/582170).
  if (enable_threaded_texture_mailboxes)
    frame->metadata()->SetBoolean(VideoFrameMetadata::COPY_REQUIRED, true);

  const bool is_surface_control =
      overlay_mode == OverlayMode::kSurfaceControlSecure ||
      overlay_mode == OverlayMode::kSurfaceControlInsecure;
  const bool wants_promotion_hints =
      overlay_mode == OverlayMode::kRequestPromotionHints;

  // Remember that we can't access |texture_owner|, but we can check if we have
  // one here.
  bool allow_overlay = false;
  if (is_surface_control) {
    DCHECK(texture_owner);
    allow_overlay = true;
  } else {
    // We unconditionally mark the picture as overlayable, even if
    // |!texture_owner|, if we want to get hints.  It's required, else we won't
    // get hints.
    allow_overlay = !texture_owner || wants_promotion_hints;
  }

  frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY,
                                allow_overlay);
  frame->metadata()->SetBoolean(VideoFrameMetadata::WANTS_PROMOTION_HINT,
                                wants_promotion_hints);
  frame->metadata()->SetBoolean(VideoFrameMetadata::TEXTURE_OWNER,
                                !!texture_owner);

  frame->SetReleaseMailboxCB(std::move(record.release_cb));

  // Note that we don't want to handle the CodecImageGroup here.  It needs to be
  // accessed on the gpu thread.  Once we move to pooling, only the initial
  // create / destroy operations will affect it anyway, so it might as well stay
  // on the gpu thread.

  std::move(output_cb).Run(std::move(frame));
}

void VideoFrameFactoryImpl::RunAfterPendingVideoFrames(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Hop through |gpu_task_runner_| to ensure it comes after pending frames.
  // TODO(liberato): If we're using a pool for SharedImageVideo, then this
  // doesn't really make much sense.  SharedImageVideoProvider should do this
  // for us instead.
  gpu_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                     std::move(closure));
}

}  // namespace media
