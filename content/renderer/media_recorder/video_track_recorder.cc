// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media_recorder/video_track_recorder.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "cc/paint/skia_paint_canvas.h"
#include "content/renderer/media/gpu/gpu_video_accelerator_factories_impl.h"
#include "content/renderer/media_recorder/vea_encoder.h"
#include "content/renderer/media_recorder/vpx_encoder.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(RTC_USE_H264)
#include "content/renderer/media_recorder/h264_encoder.h"
#endif  // #if BUILDFLAG(RTC_USE_H264)

using media::VideoFrame;
using video_track_recorder::kVEAEncoderMinResolutionWidth;
using video_track_recorder::kVEAEncoderMinResolutionHeight;

namespace content {

libyuv::RotationMode MediaVideoRotationToRotationMode(
    media::VideoRotation rotation) {
  switch (rotation) {
    case media::VIDEO_ROTATION_0:
      return libyuv::kRotate0;
    case media::VIDEO_ROTATION_90:
      return libyuv::kRotate90;
    case media::VIDEO_ROTATION_180:
      return libyuv::kRotate180;
    case media::VIDEO_ROTATION_270:
      return libyuv::kRotate270;
  }
  NOTREACHED() << rotation;
  return libyuv::kRotate0;
}

namespace {

using CodecId = VideoTrackRecorder::CodecId;

static const struct {
  CodecId codec_id;
  media::VideoCodecProfile min_profile;
  media::VideoCodecProfile max_profile;
} kPreferredCodecIdAndVEAProfiles[] = {
    {CodecId::VP8, media::VP8PROFILE_MIN, media::VP8PROFILE_MAX},
    {CodecId::VP9, media::VP9PROFILE_MIN, media::VP9PROFILE_MAX},
#if BUILDFLAG(RTC_USE_H264)
    {CodecId::H264, media::H264PROFILE_MIN, media::H264PROFILE_MAX}
#endif
};

static_assert(base::size(kPreferredCodecIdAndVEAProfiles) ==
                  static_cast<int>(CodecId::LAST),
              "|kPreferredCodecIdAndVEAProfiles| should consider all CodecIds");

// The maximum number of frames that we keep the reference alive for encode.
// This guarantees that there is limit on the number of frames in a FIFO queue
// that are being encoded and frames coming after this limit is reached are
// dropped.
// TODO(emircan): Make this a LIFO queue that has different sizes for each
// encoder implementation.
const int kMaxNumberOfFramesInEncode = 10;

// Obtains video encode accelerator's supported profiles.
media::VideoEncodeAccelerator::SupportedProfiles GetVEASupportedProfiles() {
  content::RenderThreadImpl* const render_thread_impl =
      content::RenderThreadImpl::current();
  if (!render_thread_impl) {
    DVLOG(2) << "Couldn't access the render thread";
    return media::VideoEncodeAccelerator::SupportedProfiles();
  }

  media::GpuVideoAcceleratorFactories* const gpu_factories =
      render_thread_impl->GetGpuFactories();
  if (!gpu_factories || !gpu_factories->IsGpuVideoAcceleratorEnabled()) {
    DVLOG(2) << "Couldn't initialize GpuVideoAcceleratorFactories";
    return media::VideoEncodeAccelerator::SupportedProfiles();
  }
  return gpu_factories->GetVideoEncodeAcceleratorSupportedProfiles();
}

VideoTrackRecorder::CodecEnumerator* GetCodecEnumerator() {
  static VideoTrackRecorder::CodecEnumerator* enumerator =
      new VideoTrackRecorder::CodecEnumerator(GetVEASupportedProfiles());
  return enumerator;
}

}  // anonymous namespace

VideoTrackRecorder::CodecEnumerator::CodecEnumerator(
    const media::VideoEncodeAccelerator::SupportedProfiles&
        vea_supported_profiles) {
  for (const auto& supported_profile : vea_supported_profiles) {
    const media::VideoCodecProfile codec = supported_profile.profile;
#if defined(OS_ANDROID)
    // TODO(mcasas): enable other codecs, https://crbug.com/638664.
    static_assert(media::VP8PROFILE_MAX + 1 == media::VP9PROFILE_MIN,
                  "VP8 and VP9 VideoCodecProfiles should be contiguous");
    if (codec < media::VP8PROFILE_MIN || codec > media::VP9PROFILE_MAX)
      continue;
#endif
    for (auto& codec_id_and_profile : kPreferredCodecIdAndVEAProfiles) {
      if (codec >= codec_id_and_profile.min_profile &&
          codec <= codec_id_and_profile.max_profile) {
        DVLOG(2) << "Accelerated codec found: " << media::GetProfileName(codec)
                 << ", max_resolution: "
                 << supported_profile.max_resolution.ToString()
                 << ", max_framerate: "
                 << supported_profile.max_framerate_numerator << "/"
                 << supported_profile.max_framerate_denominator;
        supported_profiles_[codec_id_and_profile.codec_id].push_back(
            supported_profile);
      }
    }
  }
}

VideoTrackRecorder::CodecEnumerator::~CodecEnumerator() = default;

VideoTrackRecorder::CodecId
VideoTrackRecorder::CodecEnumerator::GetPreferredCodecId() const {
  if (supported_profiles_.empty())
    return CodecId::VP8;
  return supported_profiles_.begin()->first;
}

media::VideoCodecProfile
VideoTrackRecorder::CodecEnumerator::GetFirstSupportedVideoCodecProfile(
    CodecId codec) const {
  const auto profile = supported_profiles_.find(codec);
  return profile == supported_profiles_.end()
             ? media::VIDEO_CODEC_PROFILE_UNKNOWN
             : profile->second.front().profile;
}

media::VideoEncodeAccelerator::SupportedProfiles
VideoTrackRecorder::CodecEnumerator::GetSupportedProfiles(CodecId codec) const {
  const auto profile = supported_profiles_.find(codec);
  return profile == supported_profiles_.end()
             ? media::VideoEncodeAccelerator::SupportedProfiles()
             : profile->second;
}

VideoTrackRecorder::Counter::Counter() : count_(0u), weak_factory_(this) {}

VideoTrackRecorder::Counter::~Counter() = default;

void VideoTrackRecorder::Counter::IncreaseCount() {
  count_++;
}

void VideoTrackRecorder::Counter::DecreaseCount() {
  count_--;
}

base::WeakPtr<VideoTrackRecorder::Counter>
VideoTrackRecorder::Counter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

VideoTrackRecorder::Encoder::Encoder(
    const OnEncodedVideoCB& on_encoded_video_callback,
    int32_t bits_per_second,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> encoding_task_runner)
    : main_task_runner_(std::move(main_task_runner)),
      encoding_task_runner_(encoding_task_runner),
      paused_(false),
      on_encoded_video_callback_(on_encoded_video_callback),
      bits_per_second_(bits_per_second),
      num_frames_in_encode_(std::make_unique<VideoTrackRecorder::Counter>()) {
  DCHECK(!on_encoded_video_callback_.is_null());
  if (encoding_task_runner_)
    return;
  encoding_thread_.reset(new base::Thread("EncodingThread"));
  encoding_thread_->Start();
  encoding_task_runner_ = encoding_thread_->task_runner();
}

VideoTrackRecorder::Encoder::~Encoder() {
  main_task_runner_->DeleteSoon(FROM_HERE, video_renderer_.release());
  if (origin_task_runner_ && !origin_task_runner_->BelongsToCurrentThread()) {
    origin_task_runner_->DeleteSoon(FROM_HERE,
                                    std::move(num_frames_in_encode_));
  }
}

void VideoTrackRecorder::Encoder::StartFrameEncode(
    scoped_refptr<VideoFrame> video_frame,
    base::TimeTicks capture_timestamp) {
  // Cache the thread sending frames on first frame arrival.
  if (!origin_task_runner_.get())
    origin_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  DCHECK(origin_task_runner_->BelongsToCurrentThread());
  if (paused_)
    return;

  if (!(video_frame->format() == media::PIXEL_FORMAT_I420 ||
        video_frame->format() == media::PIXEL_FORMAT_ARGB ||
        video_frame->format() == media::PIXEL_FORMAT_I420A ||
        video_frame->format() == media::PIXEL_FORMAT_NV12)) {
    NOTREACHED() << media::VideoPixelFormatToString(video_frame->format());
    return;
  }

  if (num_frames_in_encode_->count() > kMaxNumberOfFramesInEncode) {
    DLOG(WARNING) << "Too many frames are queued up. Dropping this one.";
    return;
  }

  if (video_frame->HasTextures()) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Encoder::RetrieveFrameOnMainThread, this,
                                  std::move(video_frame), capture_timestamp));
    return;
  }

  scoped_refptr<media::VideoFrame> wrapped_frame;
  // Drop alpha channel if the encoder does not support it yet.
  if (!CanEncodeAlphaChannel() &&
      video_frame->format() == media::PIXEL_FORMAT_I420A) {
    wrapped_frame = media::WrapAsI420VideoFrame(video_frame);
  } else {
    wrapped_frame = media::VideoFrame::WrapVideoFrame(
        *video_frame, video_frame->format(), video_frame->visible_rect(),
        video_frame->natural_size());
  }
  wrapped_frame->AddDestructionObserver(media::BindToCurrentLoop(
      base::BindOnce(&VideoTrackRecorder::Counter::DecreaseCount,
                     num_frames_in_encode_->GetWeakPtr())));
  wrapped_frame->AddDestructionObserver(base::BindOnce(
      [](scoped_refptr<VideoFrame> video_frame) {}, std::move(video_frame)));
  num_frames_in_encode_->IncreaseCount();

  encoding_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Encoder::EncodeOnEncodingTaskRunner, this,
                                wrapped_frame, capture_timestamp));
}

void VideoTrackRecorder::Encoder::RetrieveFrameOnMainThread(
    scoped_refptr<VideoFrame> video_frame,
    base::TimeTicks capture_timestamp) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  scoped_refptr<media::VideoFrame> frame;

  // |context_provider| is null if the GPU process has crashed or isn't there.
  viz::ContextProviderCommandBuffer* const context_provider =
      RenderThreadImpl::current()->SharedMainThreadContextProvider().get();
  if (!context_provider) {
    // Send black frames (yuv = {0, 127, 127}).
    frame = media::VideoFrame::CreateColorFrame(
        video_frame->visible_rect().size(), 0u, 0x80, 0x80,
        video_frame->timestamp());
  } else {
    // Accelerated decoders produce ARGB/ABGR texture-backed frames (see
    // https://crbug.com/585242), fetch them using a PaintCanvasVideoRenderer.
    DCHECK(video_frame->HasTextures());
    DCHECK_EQ(media::PIXEL_FORMAT_ARGB, video_frame->format());

    const gfx::Size& old_visible_size = video_frame->visible_rect().size();
    gfx::Size new_visible_size = old_visible_size;

    media::VideoRotation video_rotation = media::VIDEO_ROTATION_0;
    if (video_frame->metadata()->GetRotation(
            media::VideoFrameMetadata::ROTATION, &video_rotation) &&
        (video_rotation == media::VIDEO_ROTATION_90 ||
         video_rotation == media::VIDEO_ROTATION_270)) {
      new_visible_size.SetSize(old_visible_size.height(),
                               old_visible_size.width());
    }

    frame = media::VideoFrame::CreateFrame(
        media::PIXEL_FORMAT_I420, new_visible_size, gfx::Rect(new_visible_size),
        new_visible_size, video_frame->timestamp());

    const SkImageInfo info = SkImageInfo::MakeN32(
        frame->visible_rect().width(), frame->visible_rect().height(),
        kOpaque_SkAlphaType);

    // Create |surface_| if it doesn't exist or incoming resolution has changed.
    if (!canvas_ || canvas_->imageInfo().width() != info.width() ||
        canvas_->imageInfo().height() != info.height()) {
      bitmap_.allocPixels(info);
      canvas_ = std::make_unique<cc::SkiaPaintCanvas>(bitmap_);
    }
    if (!video_renderer_)
      video_renderer_.reset(new media::PaintCanvasVideoRenderer);

    video_renderer_->Copy(video_frame.get(), canvas_.get(), context_provider);

    SkPixmap pixmap;
    if (!bitmap_.peekPixels(&pixmap)) {
      DLOG(ERROR) << "Error trying to map PaintSurface's pixels";
      return;
    }

    const uint32_t source_pixel_format =
        (kN32_SkColorType == kRGBA_8888_SkColorType) ? libyuv::FOURCC_ABGR
                                                     : libyuv::FOURCC_ARGB;
    if (libyuv::ConvertToI420(static_cast<uint8_t*>(pixmap.writable_addr()),
                              pixmap.computeByteSize(),
                              frame->visible_data(media::VideoFrame::kYPlane),
                              frame->stride(media::VideoFrame::kYPlane),
                              frame->visible_data(media::VideoFrame::kUPlane),
                              frame->stride(media::VideoFrame::kUPlane),
                              frame->visible_data(media::VideoFrame::kVPlane),
                              frame->stride(media::VideoFrame::kVPlane),
                              0 /* crop_x */, 0 /* crop_y */, pixmap.width(),
                              pixmap.height(), old_visible_size.width(),
                              old_visible_size.height(),
                              MediaVideoRotationToRotationMode(video_rotation),
                              source_pixel_format) != 0) {
      DLOG(ERROR) << "Error converting frame to I420";
      return;
    }
  }

  encoding_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Encoder::EncodeOnEncodingTaskRunner, this,
                                frame, capture_timestamp));
}

// static
void VideoTrackRecorder::Encoder::OnFrameEncodeCompleted(
    const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_cb,
    const media::WebmMuxer::VideoParameters& params,
    std::unique_ptr<std::string> data,
    std::unique_ptr<std::string> alpha_data,
    base::TimeTicks capture_timestamp,
    bool keyframe) {
  DVLOG(1) << (keyframe ? "" : "non ") << "keyframe "<< data->length() << "B, "
           << capture_timestamp << " ms";
  on_encoded_video_cb.Run(params, std::move(data), std::move(alpha_data),
                          capture_timestamp, keyframe);
}

void VideoTrackRecorder::Encoder::SetPaused(bool paused) {
  if (!encoding_task_runner_->BelongsToCurrentThread()) {
    encoding_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Encoder::SetPaused, this, paused));
    return;
  }
  paused_ = paused;
}

bool VideoTrackRecorder::Encoder::CanEncodeAlphaChannel() {
  return false;
}

// static
VideoTrackRecorder::CodecId VideoTrackRecorder::GetPreferredCodecId() {
  return GetCodecEnumerator()->GetPreferredCodecId();
}

// static
bool VideoTrackRecorder::CanUseAcceleratedEncoder(CodecId codec,
                                                  size_t width,
                                                  size_t height,
                                                  double framerate) {
  const auto profiles = GetCodecEnumerator()->GetSupportedProfiles(codec);
  if (profiles.empty())
    return false;

  // Now we only consider the first profile.
  // TODO(crbug.com/931035): Handle multiple profile cases.
  const media::VideoEncodeAccelerator::SupportedProfile& profile = profiles[0];

  if (profile.profile == media::VIDEO_CODEC_PROFILE_UNKNOWN)
    return false;

  const gfx::Size& max_resolution = profile.max_resolution;
  DCHECK_GE(max_resolution.width(), 0);
  const size_t max_width = static_cast<size_t>(max_resolution.width());
  DCHECK_GE(max_resolution.height(), 0);
  const size_t max_height = static_cast<size_t>(max_resolution.height());

  const bool width_within_range =
      max_width >= width && width >= kVEAEncoderMinResolutionWidth;
  const bool height_within_range =
      max_height >= height && height >= kVEAEncoderMinResolutionHeight;
  const bool valid_framerate = framerate * profile.max_framerate_denominator <=
                               profile.max_framerate_numerator;
  return width_within_range && height_within_range && valid_framerate;
}

VideoTrackRecorder::VideoTrackRecorder(
    CodecId codec,
    const blink::WebMediaStreamTrack& track,
    const OnEncodedVideoCB& on_encoded_video_callback,
    int32_t bits_per_second,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : track_(track),
      should_pause_encoder_on_initialization_(false),
      main_task_runner_(std::move(main_task_runner)),
      weak_ptr_factory_(this) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(!track_.IsNull());
  DCHECK(track_.GetPlatformTrack());

  initialize_encoder_callback_ = base::Bind(
      &VideoTrackRecorder::InitializeEncoder, weak_ptr_factory_.GetWeakPtr(),
      codec, on_encoded_video_callback, bits_per_second);

  // InitializeEncoder() will be called on Render Main thread.
  blink::MediaStreamVideoSink::ConnectToTrack(
      track_,
      media::BindToCurrentLoop(base::Bind(initialize_encoder_callback_,
                                          true /* allow_vea_encoder */)),
      false);
}

VideoTrackRecorder::~VideoTrackRecorder() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  blink::MediaStreamVideoSink::DisconnectFromTrack();
  track_.Reset();
}

void VideoTrackRecorder::Pause() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (encoder_)
    encoder_->SetPaused(true);
  else
    should_pause_encoder_on_initialization_ = true;
}

void VideoTrackRecorder::Resume() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (encoder_)
    encoder_->SetPaused(false);
  else
    should_pause_encoder_on_initialization_ = false;
}

void VideoTrackRecorder::OnVideoFrameForTesting(
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks timestamp) {
  DVLOG(3) << __func__;

  if (!encoder_) {
    DCHECK(!initialize_encoder_callback_.is_null());
    initialize_encoder_callback_.Run(true /* allow_vea_encoder */, frame,
                                     timestamp);
  }

  encoder_->StartFrameEncode(std::move(frame), timestamp);
}

void VideoTrackRecorder::InitializeEncoder(
    CodecId codec,
    const OnEncodedVideoCB& on_encoded_video_callback,
    int32_t bits_per_second,
    bool allow_vea_encoder,
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks capture_time) {
  DVLOG(3) << __func__ << frame->visible_rect().size().ToString();
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  // Avoid reinitializing |encoder_| when there are multiple frames sent to the
  // sink to initialize, https://crbug.com/698441.
  if (encoder_)
    return;

  blink::MediaStreamVideoSink::DisconnectFromTrack();

  const gfx::Size& input_size = frame->visible_rect().size();
  if (allow_vea_encoder && CanUseAcceleratedEncoder(codec, input_size.width(),
                                                    input_size.height())) {
    UMA_HISTOGRAM_BOOLEAN("Media.MediaRecorder.VEAUsed", true);
    const auto vea_profile =
        GetCodecEnumerator()->GetFirstSupportedVideoCodecProfile(codec);
    encoder_ = VEAEncoder::Create(
        on_encoded_video_callback,
        media::BindToCurrentLoop(base::Bind(&VideoTrackRecorder::OnError,
                                            weak_ptr_factory_.GetWeakPtr())),
        bits_per_second, vea_profile, input_size, main_task_runner_);
  } else {
    UMA_HISTOGRAM_BOOLEAN("Media.MediaRecorder.VEAUsed", false);
    switch (codec) {
#if BUILDFLAG(RTC_USE_H264)
      case CodecId::H264:
        encoder_ = new H264Encoder(on_encoded_video_callback, bits_per_second,
                                   main_task_runner_);
        break;
#endif
      case CodecId::VP8:
      case CodecId::VP9:
        encoder_ =
            new VpxEncoder(codec == CodecId::VP9, on_encoded_video_callback,
                           bits_per_second, main_task_runner_);
        break;
      default:
        NOTREACHED() << "Unsupported codec " << static_cast<int>(codec);
    }
  }

  if (should_pause_encoder_on_initialization_)
    encoder_->SetPaused(should_pause_encoder_on_initialization_);

  // StartFrameEncode() will be called on Render IO thread.
  blink::MediaStreamVideoSink::ConnectToTrack(
      track_,
      base::Bind(&VideoTrackRecorder::Encoder::StartFrameEncode, encoder_),
      false);
}

void VideoTrackRecorder::OnError() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  // InitializeEncoder() will be called to reinitialize encoder on Render Main
  // thread.
  blink::MediaStreamVideoSink::DisconnectFromTrack();
  encoder_ = nullptr;
  blink::MediaStreamVideoSink::ConnectToTrack(
      track_,
      media::BindToCurrentLoop(base::Bind(initialize_encoder_callback_,
                                          false /*allow_vea_encoder*/)),
      false);
}

}  // namespace content
