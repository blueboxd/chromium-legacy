// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediacapturefromelement/html_video_element_capturer_source.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/skia_paint_canvas.h"
#include "media/base/limits.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/platform/modules/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/libyuv/include/libyuv.h"

namespace {
const float kMinFramesPerSecond = 1.0;
}  // anonymous namespace

namespace blink {

// static
std::unique_ptr<HtmlVideoElementCapturerSource>
HtmlVideoElementCapturerSource::CreateFromWebMediaPlayerImpl(
    blink::WebMediaPlayer* player,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  // Save histogram data so we can see how much HTML Video capture is used.
  // The histogram counts the number of calls to the JS API.
  UpdateWebRTCMethodCount(blink::WebRTCAPIName::kVideoCaptureStream);

  // TODO(crbug.com/963651): Remove the need for AsWeakPtr altogether.
  return base::WrapUnique(new HtmlVideoElementCapturerSource(
      player->AsWeakPtr(), io_task_runner, task_runner));
}

HtmlVideoElementCapturerSource::HtmlVideoElementCapturerSource(
    const base::WeakPtr<blink::WebMediaPlayer>& player,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : web_media_player_(player),
      io_task_runner_(io_task_runner),
      task_runner_(task_runner),
      is_opaque_(player->IsOpaque()),
      capture_frame_rate_(0.0) {
  DCHECK(web_media_player_);
}

HtmlVideoElementCapturerSource::~HtmlVideoElementCapturerSource() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

media::VideoCaptureFormats
HtmlVideoElementCapturerSource::GetPreferredFormats() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // WebMediaPlayer has a setRate() but can't be read back.
  // TODO(mcasas): Add getRate() to WMPlayer and/or fix the spec to allow users
  // to specify it.
  const media::VideoCaptureFormat format(
      gfx::Size(web_media_player_->NaturalSize()),
      blink::MediaStreamVideoSource::kDefaultFrameRate,
      media::PIXEL_FORMAT_I420);
  media::VideoCaptureFormats formats;
  formats.push_back(format);
  return formats;
}

void HtmlVideoElementCapturerSource::StartCapture(
    const media::VideoCaptureParams& params,
    const VideoCaptureDeliverFrameCB& new_frame_callback,
    const RunningCallback& running_callback) {
  DVLOG(2) << __func__ << " requested "
           << media::VideoCaptureFormat::ToString(params.requested_format);
  DCHECK(params.requested_format.IsValid());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  running_callback_ = running_callback;
  if (!web_media_player_ || !web_media_player_->HasVideo()) {
    running_callback_.Run(false);
    return;
  }

  new_frame_callback_ = new_frame_callback;
  // Force |capture_frame_rate_| to be in between k{Min,Max}FramesPerSecond.
  capture_frame_rate_ =
      std::max(kMinFramesPerSecond,
               std::min(static_cast<float>(media::limits::kMaxFramesPerSecond),
                        params.requested_format.frame_rate));

  running_callback_.Run(true);
  // TODO(crbug.com/964463): Use per-frame task runner.
  Thread::Current()->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&HtmlVideoElementCapturerSource::sendNewFrame,
                           weak_factory_.GetWeakPtr()));
}

void HtmlVideoElementCapturerSource::StopCapture() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  running_callback_.Reset();
  new_frame_callback_.Reset();
  next_capture_time_ = base::TimeTicks();
}

void HtmlVideoElementCapturerSource::sendNewFrame() {
  DVLOG(3) << __func__;
  TRACE_EVENT0("media", "HtmlVideoElementCapturerSource::sendNewFrame");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!web_media_player_ || new_frame_callback_.is_null())
    return;

  const base::TimeTicks current_time = base::TimeTicks::Now();
  if (start_capture_time_.is_null())
    start_capture_time_ = current_time;
  const blink::WebSize resolution = web_media_player_->NaturalSize();

  if (!canvas_ || is_opaque_ != web_media_player_->IsOpaque()) {
    is_opaque_ = web_media_player_->IsOpaque();
    if (!bitmap_.tryAllocPixels(SkImageInfo::MakeN32(
            resolution.width, resolution.height,
            is_opaque_ ? kOpaque_SkAlphaType : kPremul_SkAlphaType))) {
      running_callback_.Run(false);
      return;
    }
    canvas_ = std::make_unique<cc::SkiaPaintCanvas>(bitmap_);
  }

  cc::PaintFlags flags;
  flags.setBlendMode(SkBlendMode::kSrc);
  flags.setFilterQuality(kLow_SkFilterQuality);
  web_media_player_->Paint(
      canvas_.get(), blink::WebRect(0, 0, resolution.width, resolution.height),
      flags);
  DCHECK_NE(kUnknown_SkColorType, canvas_->imageInfo().colorType());
  DCHECK_EQ(canvas_->imageInfo().width(), resolution.width);
  DCHECK_EQ(canvas_->imageInfo().height(), resolution.height);

  DCHECK_NE(kUnknown_SkColorType, bitmap_.colorType());
  DCHECK(!bitmap_.drawsNothing());
  DCHECK(bitmap_.getPixels());
  if (bitmap_.colorType() != kN32_SkColorType) {
    DLOG(ERROR) << "Only supported color type is kN32_SkColorType (ARGB/ABGR)";
    return;
  }

  // TODO(crbug.com/964494): Avoid the explicit convertion to gfx::Size here.
  gfx::Size gfx_resolution = gfx::Size(resolution);
  scoped_refptr<media::VideoFrame> frame = frame_pool_.CreateFrame(
      is_opaque_ ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
      gfx_resolution, gfx::Rect(gfx_resolution), gfx_resolution,
      current_time - start_capture_time_);

  const uint32_t source_pixel_format =
      (kN32_SkColorType == kRGBA_8888_SkColorType) ? libyuv::FOURCC_ABGR
                                                   : libyuv::FOURCC_ARGB;

  if (frame &&
      libyuv::ConvertToI420(
          static_cast<uint8_t*>(bitmap_.getPixels()), bitmap_.computeByteSize(),
          frame->visible_data(media::VideoFrame::kYPlane),
          frame->stride(media::VideoFrame::kYPlane),
          frame->visible_data(media::VideoFrame::kUPlane),
          frame->stride(media::VideoFrame::kUPlane),
          frame->visible_data(media::VideoFrame::kVPlane),
          frame->stride(media::VideoFrame::kVPlane), 0 /* crop_x */,
          0 /* crop_y */, frame->visible_rect().size().width(),
          frame->visible_rect().size().height(), bitmap_.info().width(),
          bitmap_.info().height(), libyuv::kRotate0,
          source_pixel_format) == 0) {
    if (!is_opaque_) {
      // OK to use ARGB...() because alpha has the same alignment for both ABGR
      // and ARGB.
      libyuv::ARGBExtractAlpha(
          static_cast<uint8_t*>(bitmap_.getPixels()),
          static_cast<int>(bitmap_.rowBytes()) /* stride */,
          frame->visible_data(media::VideoFrame::kAPlane),
          frame->stride(media::VideoFrame::kAPlane), bitmap_.info().width(),
          bitmap_.info().height());
    }  // Success!

    // Post with CrossThreadBind here, instead of CrossThreadBindOnce,
    // otherwise the |new_frame_callback_| ivar can be nulled out
    // unintentionally.
    //
    // TODO(crbug.com/964922): Consider cloning |new_frame_callback_|
    // and use CrossThreadBind
    PostCrossThreadTask(
        *io_task_runner_, FROM_HERE,
        CrossThreadBindRepeating(new_frame_callback_, frame, current_time));
  }

  // Calculate the time in the future where the next frame should be created.
  const base::TimeDelta frame_interval =
      base::TimeDelta::FromMicroseconds(1E6 / capture_frame_rate_);
  if (next_capture_time_.is_null()) {
    next_capture_time_ = current_time + frame_interval;
  } else {
    next_capture_time_ += frame_interval;
    // Don't accumulate any debt if we are lagging behind - just post next frame
    // immediately and continue as normal.
    if (next_capture_time_ < current_time)
      next_capture_time_ = current_time;
  }
  // Schedule next capture.
  PostDelayedCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(&HtmlVideoElementCapturerSource::sendNewFrame,
                          weak_factory_.GetWeakPtr()),
      next_capture_time_ - current_time);
}

}  // namespace blink
