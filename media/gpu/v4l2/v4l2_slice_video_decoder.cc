// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_slice_video_decoder.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "media/base/scopedfd_helper.h"
#include "media/base/video_util.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/linux/dmabuf_video_frame_pool.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_h264_accelerator.h"
#include "media/gpu/v4l2/v4l2_vp8_accelerator.h"
#include "media/gpu/v4l2/v4l2_vp9_accelerator.h"

namespace media {

namespace {

// See http://crbug.com/255116.
constexpr int k1080pArea = 1920 * 1088;
// Input bitstream buffer size for up to 1080p streams.
constexpr size_t kInputBufferMaxSizeFor1080p = 1024 * 1024;
// Input bitstream buffer size for up to 4k streams.
constexpr size_t kInputBufferMaxSizeFor4k = 4 * kInputBufferMaxSizeFor1080p;
constexpr size_t kNumInputBuffers = 16;
constexpr size_t kNumInputPlanes = 1;

// Input format V4L2 fourccs this class supports.
constexpr uint32_t kSupportedInputFourccs[] = {
    V4L2_PIX_FMT_H264_SLICE,
    V4L2_PIX_FMT_VP8_FRAME,
    V4L2_PIX_FMT_VP9_FRAME,
};

// Checks an underlying video frame buffer of |frame| is valid for VIDIOC_DQBUF
// that requires |target_num_fds| fds.
bool IsValidFrameForQueueDMABuf(const VideoFrame* frame,
                                size_t target_num_fds) {
  DCHECK(frame);
  if (frame->DmabufFds().size() < target_num_fds) {
    VLOGF(1) << "The count of dmabuf fds (" << frame->DmabufFds().size()
             << ") are not enough, needs " << target_num_fds << " fds.";
    return false;
  }

  const std::vector<VideoFrameLayout::Plane>& planes = frame->layout().planes();
  for (size_t i = frame->DmabufFds().size() - 1; i >= target_num_fds; --i) {
    // Assume that an fd is a duplicate of a previous plane's fd if offset != 0.
    // Otherwise, if offset == 0, return error as surface_it may be pointing to
    // a new plane.
    if (planes[i].offset == 0) {
      VLOGF(1) << "Additional dmabuf fds point to a new buffer.";
      return false;
    }
  }

  return true;
}

}  // namespace

struct V4L2SliceVideoDecoder::OutputRequest {
  enum OutputRequestType {
    // The surface to be outputted.
    kSurface,
    // The fence to indicate the flush request.
    kFlushFence,
    // The fence to indicate resolution change request.
    kChangeResolutionFence,
  };

  // The type of the request.
  const OutputRequestType type;
  // The surface to be outputted.
  scoped_refptr<V4L2DecodeSurface> surface;
  // The timestamp of the output frame. Because a surface might be outputted
  // multiple times with different timestamp, we need to store timestamp out of
  // surface.
  base::TimeDelta timestamp;

  static OutputRequest Surface(scoped_refptr<V4L2DecodeSurface> s,
                               base::TimeDelta t) {
    return OutputRequest(std::move(s), t);
  }

  static OutputRequest FlushFence() { return OutputRequest(kFlushFence); }

  static OutputRequest ChangeResolutionFence() {
    return OutputRequest(kChangeResolutionFence);
  }

  bool IsReady() const {
    return (type != OutputRequestType::kSurface) || surface->decoded();
  }

  // Allow move, but not copy.
  OutputRequest(OutputRequest&&) = default;

 private:
  OutputRequest(scoped_refptr<V4L2DecodeSurface> s, base::TimeDelta t)
      : type(kSurface), surface(std::move(s)), timestamp(t) {}
  explicit OutputRequest(OutputRequestType t) : type(t) {}

  DISALLOW_COPY_AND_ASSIGN(OutputRequest);
};

// static
std::unique_ptr<VideoDecoder> V4L2SliceVideoDecoder::Create(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    std::unique_ptr<DmabufVideoFramePool> frame_pool) {
  DCHECK(client_task_runner->RunsTasksInCurrentSequence());
  DCHECK(frame_pool);

  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device) {
    VLOGF(1) << "Failed to create V4L2 device.";
    return nullptr;
  }

  return base::WrapUnique<VideoDecoder>(new V4L2SliceVideoDecoder(
      std::move(client_task_runner), std::move(device), std::move(frame_pool)));
}

// static
SupportedVideoDecoderConfigs V4L2SliceVideoDecoder::GetSupportedConfigs() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device)
    return SupportedVideoDecoderConfigs();

  return ConvertFromSupportedProfiles(
      device->GetSupportedDecodeProfiles(base::size(kSupportedInputFourccs),
                                         kSupportedInputFourccs),
      false);
}

V4L2SliceVideoDecoder::V4L2SliceVideoDecoder(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    scoped_refptr<V4L2Device> device,
    std::unique_ptr<DmabufVideoFramePool> frame_pool)
    : device_(std::move(device)),
      frame_pool_(std::move(frame_pool)),
      client_task_runner_(std::move(client_task_runner)),
      decoder_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::WithBaseSyncPrimitives(), base::TaskPriority::USER_VISIBLE})),
      device_poll_thread_("V4L2SliceVideoDecoderDevicePollThread"),
      weak_this_factory_(this) {
  DETACH_FROM_SEQUENCE(client_sequence_checker_);
  DETACH_FROM_SEQUENCE(decoder_sequence_checker_);
  VLOGF(2);
  weak_this_ = weak_this_factory_.GetWeakPtr();

  frame_pool_->set_parent_task_runner(decoder_task_runner_);
}

V4L2SliceVideoDecoder::~V4L2SliceVideoDecoder() {
  // We might be called from either the client or the decoder sequence.
  DETACH_FROM_SEQUENCE(client_sequence_checker_);
  DETACH_FROM_SEQUENCE(decoder_sequence_checker_);
  VLOGF(2);
}

std::string V4L2SliceVideoDecoder::GetDisplayName() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  return "V4L2SliceVideoDecoder";
}

bool V4L2SliceVideoDecoder::IsPlatformDecoder() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  return true;
}

int V4L2SliceVideoDecoder::GetMaxDecodeRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  return 4;
}

bool V4L2SliceVideoDecoder::NeedsBitstreamConversion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  return needs_bitstream_conversion_;
}

bool V4L2SliceVideoDecoder::CanReadWithoutStalling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  return frame_pool_ && !frame_pool_->IsExhausted();
}

void V4L2SliceVideoDecoder::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  VLOGF(2);

  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2SliceVideoDecoder::DestroyTask, weak_this_));
}

void V4L2SliceVideoDecoder::DestroyTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(2);

  if (avd_) {
    avd_->Reset();
    avd_ = nullptr;
  }

  // Call all pending decode callback.
  ClearPendingRequests(DecodeStatus::ABORTED);

  // Stop and Destroy device.
  StopStreamV4L2Queue();
  input_queue_->DeallocateBuffers();
  output_queue_->DeallocateBuffers();
  DCHECK(surfaces_at_device_.empty());

  weak_this_factory_.InvalidateWeakPtrs();
  delete this;
  VLOGF(2) << "Destroyed";
}

void V4L2SliceVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                       bool low_delay,
                                       CdmContext* cdm_context,
                                       InitCB init_cb,
                                       const OutputCB& output_cb,
                                       const WaitingCB& /* waiting_cb */) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  VLOGF(2) << "config: " << config.AsHumanReadableString();

  if (!config.IsValidConfig()) {
    VLOGF(1) << "config is not valid";
    std::move(init_cb).Run(false);
    return;
  }
  if (cdm_context) {
    VLOGF(1) << "cdm_context is not supported.";
    std::move(init_cb).Run(false);
    return;
  }

  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2SliceVideoDecoder::InitializeTask, weak_this_, config,
                     std::move(init_cb), std::move(output_cb)));
}

void V4L2SliceVideoDecoder::InitializeTask(const VideoDecoderConfig& config,
                                           InitCB init_cb,
                                           const OutputCB& output_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(state_ == State::kUninitialized || state_ == State::kDecoding);
  DVLOGF(3);

  if (!output_request_queue_.empty() || flush_cb_ || current_decode_request_ ||
      !decode_request_queue_.empty()) {
    VLOGF(1) << "Should not call Initialize() during pending decode";
    client_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(std::move(init_cb), false));
    return;
  }

  // Reset V4L2 device and queue if reinitializing decoder.
  if (state_ != State::kUninitialized) {
    if (!StopStreamV4L2Queue()) {
      client_task_runner_->PostTask(FROM_HERE,
                                    base::BindOnce(std::move(init_cb), false));
      return;
    }

    input_queue_->DeallocateBuffers();
    output_queue_->DeallocateBuffers();
    input_queue_ = nullptr;
    output_queue_ = nullptr;

    device_ = V4L2Device::Create();
    if (!device_) {
      VLOGF(1) << "Failed to create V4L2 device.";
      client_task_runner_->PostTask(FROM_HERE,
                                    base::BindOnce(std::move(init_cb), false));
      return;
    }

    if (avd_) {
      avd_->Reset();
      avd_ = nullptr;
    }
    SetState(State::kUninitialized);
  }

  // Open V4L2 device.
  VideoCodecProfile profile = config.profile();
  uint32_t input_format_fourcc =
      V4L2Device::VideoCodecProfileToV4L2PixFmt(profile, true);
  if (!device_->Open(V4L2Device::Type::kDecoder, input_format_fourcc)) {
    VLOGF(1) << "Failed to open device for profile: " << profile
             << " fourcc: " << FourccToString(input_format_fourcc);
    client_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(std::move(init_cb), false));
    return;
  }

  struct v4l2_capability caps;
  const __u32 kCapsRequired = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
  if (device_->Ioctl(VIDIOC_QUERYCAP, &caps) ||
      (caps.capabilities & kCapsRequired) != kCapsRequired) {
    VLOGF(1) << "ioctl() failed: VIDIOC_QUERYCAP, "
             << "caps check failed: 0x" << std::hex << caps.capabilities;
    client_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(std::move(init_cb), false));
    return;
  }

  // Create codec-specific AcceleratedVideoDecoder.
  // TODO(akahuang): Check the profile is supported.
  if (profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX) {
    avd_.reset(new H264Decoder(
        std::make_unique<V4L2H264Accelerator>(this, device_.get())));
  } else if (profile >= VP8PROFILE_MIN && profile <= VP8PROFILE_MAX) {
    avd_.reset(new VP8Decoder(
        std::make_unique<V4L2VP8Accelerator>(this, device_.get())));
  } else if (profile >= VP9PROFILE_MIN && profile <= VP9PROFILE_MAX) {
    avd_.reset(new VP9Decoder(
        std::make_unique<V4L2VP9Accelerator>(this, device_.get())));
  } else {
    VLOGF(1) << "Unsupported profile " << GetProfileName(profile);
    client_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(std::move(init_cb), false));
    return;
  }

  needs_bitstream_conversion_ = (config.codec() == kCodecH264);
  pixel_aspect_ratio_ = config.GetPixelAspectRatio();

  // Setup input format.
  if (!SetupInputFormat(input_format_fourcc)) {
    VLOGF(1) << "Failed to setup input format.";
    client_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(std::move(init_cb), false));
    return;
  }

  // Setup output format.
  if (!SetupOutputFormat(config.coded_size(), config.visible_rect())) {
    VLOGF(1) << "Failed to setup output format.";
    client_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(std::move(init_cb), false));
    return;
  }

  // Create Input/Output V4L2Queue
  input_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  output_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  if (!input_queue_ || !output_queue_) {
    VLOGF(1) << "Failed to create V4L2 queue.";
    client_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(std::move(init_cb), false));
    return;
  }
  if (input_queue_->AllocateBuffers(kNumInputBuffers, V4L2_MEMORY_MMAP) == 0) {
    VLOGF(1) << "Failed to allocate input buffer.";
    client_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(std::move(init_cb), false));
    return;
  }

  // Call init_cb
  output_cb_ = output_cb;
  SetState(State::kDecoding);
  client_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(std::move(init_cb), true));
}

bool V4L2SliceVideoDecoder::SetupInputFormat(uint32_t input_format_fourcc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK_EQ(state_, State::kUninitialized);

  // Check if the format is supported.
  std::vector<uint32_t> formats = device_->EnumerateSupportedPixelformats(
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  if (std::find(formats.begin(), formats.end(), input_format_fourcc) ==
      formats.end()) {
    DVLOGF(3) << "Input fourcc " << input_format_fourcc
              << " not supported by device.";
    return false;
  }

  // Determine the input buffer size.
  gfx::Size max_size, min_size;
  device_->GetSupportedResolution(input_format_fourcc, &min_size, &max_size);
  size_t input_size = max_size.GetArea() > k1080pArea
                          ? kInputBufferMaxSizeFor4k
                          : kInputBufferMaxSizeFor1080p;

  // Setup the input format.
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.pixelformat = input_format_fourcc;
  format.fmt.pix_mp.plane_fmt[0].sizeimage = input_size;
  format.fmt.pix_mp.num_planes = kNumInputPlanes;
  if (device_->Ioctl(VIDIOC_S_FMT, &format) != 0) {
    VPLOGF(1) << "Failed to call IOCTL to set input format.";
    return false;
  }
  DCHECK_EQ(format.fmt.pix_mp.pixelformat, input_format_fourcc);

  return true;
}

base::Optional<struct v4l2_format>
V4L2SliceVideoDecoder::SetFormatOnOutputQueue(uint32_t format_fourcc,
                                              const gfx::Size& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  struct v4l2_format format = {};
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.pixelformat = format_fourcc;
  format.fmt.pix_mp.width = size.width();
  format.fmt.pix_mp.height = size.height();
  format.fmt.pix_mp.num_planes =
      V4L2Device::GetNumPlanesOfV4L2PixFmt(format_fourcc);
  if (device_->Ioctl(VIDIOC_S_FMT, &format) != 0 ||
      format.fmt.pix_mp.pixelformat != format_fourcc) {
    VPLOGF(2) << "Failed to set output format. format_fourcc=" << format_fourcc;
    return base::nullopt;
  }
  return format;
}

base::Optional<VideoFrameLayout> V4L2SliceVideoDecoder::SetupOutputFormat(
    const gfx::Size& size,
    const gfx::Rect& visible_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  const std::vector<uint32_t> formats = device_->EnumerateSupportedPixelformats(
      V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  DCHECK(!formats.empty());
  for (const auto format_fourcc : formats) {
    if (!device_->CanCreateEGLImageFrom(format_fourcc))
      continue;

    // Make sure VFPool can allocate video frames with width and height.
    auto frame_layout =
        UpdateVideoFramePoolFormat(format_fourcc, size, visible_rect);
    if (!frame_layout) {
      continue;
    }

    // Next S_FMT with the size adjusted by VFPool.
    gfx::Size adjusted_size(frame_layout->planes()[0].stride,
                            frame_layout->coded_size().height());
    base::Optional<struct v4l2_format> format =
        SetFormatOnOutputQueue(format_fourcc, adjusted_size);
    if (!format) {
      num_output_planes_ = format->fmt.pix_mp.num_planes;
      return frame_layout;
    }
  }

  // TODO(akahuang): Use ImageProcessor in this case.
  VLOGF(2) << "WARNING: Cannot find format that can create EGL image. "
           << "We need ImageProcessor to convert pixel format.";
  NOTIMPLEMENTED();
  return base::nullopt;
}

base::Optional<VideoFrameLayout>
V4L2SliceVideoDecoder::UpdateVideoFramePoolFormat(
    uint32_t output_format_fourcc,
    const gfx::Size& size,
    const gfx::Rect& visible_rect) {
  VideoPixelFormat output_format =
      V4L2Device::V4L2PixFmtToVideoPixelFormat(output_format_fourcc);
  if (output_format == PIXEL_FORMAT_UNKNOWN) {
    return base::nullopt;
  }
  auto layout = VideoFrameLayout::Create(output_format, size);
  if (!layout) {
    VLOGF(1) << "Failed to create video frame layout.";
    return base::nullopt;
  }

  gfx::Size natural_size = GetNaturalSize(visible_rect, pixel_aspect_ratio_);
  return frame_pool_->NegotiateFrameFormat(*layout, visible_rect, natural_size);
}

void V4L2SliceVideoDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DVLOGF(3);

  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2SliceVideoDecoder::ResetTask, weak_this_,
                                std::move(closure)));
}

void V4L2SliceVideoDecoder::ResetTask(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  if (avd_)
    avd_->Reset();

  // Call all pending decode callback.
  ClearPendingRequests(DecodeStatus::ABORTED);

  // Streamoff V4L2 queues to drop input and output buffers.
  // If the queues are streaming before reset, then we need to start streaming
  // them after stopping.
  bool poll_thread_running = device_poll_thread_.IsRunning();
  if (!StopStreamV4L2Queue())
    return;

  if (poll_thread_running) {
    if (!StartStreamV4L2Queue())
      return;
  }

  client_task_runner_->PostTask(FROM_HERE, std::move(closure));
}

void V4L2SliceVideoDecoder::ClearPendingRequests(DecodeStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  // Clear output_request_queue_.
  while (!output_request_queue_.empty())
    output_request_queue_.pop();

  if (flush_cb_)
    RunDecodeCB(std::move(flush_cb_), status);

  // Clear current_decode_request_ and decode_request_queue_.
  if (current_decode_request_) {
    RunDecodeCB(std::move(current_decode_request_->decode_cb), status);
    current_decode_request_ = base::nullopt;
  }

  while (!decode_request_queue_.empty()) {
    auto request = std::move(decode_request_queue_.front());
    decode_request_queue_.pop();
    RunDecodeCB(std::move(request.decode_cb), status);
  }
}

void V4L2SliceVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                   DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2SliceVideoDecoder::EnqueueDecodeTask, weak_this_,
                     DecodeRequest(std::move(buffer), std::move(decode_cb),
                                   GetNextBitstreamId())));
}

void V4L2SliceVideoDecoder::EnqueueDecodeTask(DecodeRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(state_ == State::kDecoding || state_ == State::kFlushing);

  if (!request.buffer->end_of_stream()) {
    bitstream_id_to_timestamp_.emplace(request.bitstream_id,
                                       request.buffer->timestamp());
  }
  decode_request_queue_.push(std::move(request));
  // If we are already decoding, then we don't need to pump again.
  if (!current_decode_request_)
    PumpDecodeTask();
}

void V4L2SliceVideoDecoder::PumpDecodeTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(state_ == State::kDecoding || state_ == State::kFlushing);
  DVLOGF(3) << "state_:" << static_cast<int>(state_)
            << " Number of Decode requests: " << decode_request_queue_.size();

  if (state_ == State::kFlushing)
    return;

  pause_reason_ = PauseReason::kNone;
  while (true) {
    switch (avd_->Decode()) {
      case AcceleratedVideoDecoder::kAllocateNewSurfaces:
        DVLOGF(3) << "Need to change resolution. Pause decoding.";
        SetState(State::kFlushing);

        output_request_queue_.push(OutputRequest::ChangeResolutionFence());
        PumpOutputSurfaces();
        return;

      case AcceleratedVideoDecoder::kRanOutOfStreamData:
        // Current decode request is finished processing.
        if (current_decode_request_) {
          DCHECK(current_decode_request_->decode_cb);
          RunDecodeCB(std::move(current_decode_request_->decode_cb),
                      DecodeStatus::OK);
          current_decode_request_ = base::nullopt;
        }

        // Process next decodee request.
        if (decode_request_queue_.empty())
          return;
        current_decode_request_ = std::move(decode_request_queue_.front());
        decode_request_queue_.pop();

        if (current_decode_request_->buffer->end_of_stream()) {
          if (!avd_->Flush()) {
            VLOGF(1) << "Failed flushing the decoder.";
            SetState(State::kError);
            return;
          }
          // Put the decoder in an idle state, ready to resume.
          avd_->Reset();

          SetState(State::kFlushing);
          DCHECK(!flush_cb_);
          flush_cb_ = std::move(current_decode_request_->decode_cb);

          output_request_queue_.push(OutputRequest::FlushFence());
          PumpOutputSurfaces();
          current_decode_request_ = base::nullopt;
          return;
        }

        avd_->SetStream(current_decode_request_->bitstream_id,
                        *current_decode_request_->buffer);
        break;

      case AcceleratedVideoDecoder::kRanOutOfSurfaces:
        DVLOGF(3) << "Ran out of surfaces. Resume when buffer is returned.";
        pause_reason_ = PauseReason::kRanOutOfSurfaces;
        return;

      case AcceleratedVideoDecoder::kNeedContextUpdate:
        DVLOGF(3) << "Awaiting context update";
        pause_reason_ = PauseReason::kWaitSubFrameDecoded;
        return;

      case AcceleratedVideoDecoder::kDecodeError:
        DVLOGF(3) << "Error decoding stream";
        SetState(State::kError);
        return;

      case AcceleratedVideoDecoder::kTryAgain:
        NOTREACHED() << "Should not reach here unless this class accepts "
                        "encrypted streams.";
        DVLOGF(4) << "No key for decoding stream.";
        SetState(State::kError);
        return;
    }
  }
}

void V4L2SliceVideoDecoder::PumpOutputSurfaces() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3) << "state_: " << static_cast<int>(state_)
            << " Number of display surfaces: " << output_request_queue_.size();

  bool resume_decode = false;
  while (!output_request_queue_.empty()) {
    if (!output_request_queue_.front().IsReady()) {
      DVLOGF(3) << "The first surface is not ready yet.";
      break;
    }

    OutputRequest request = std::move(output_request_queue_.front());
    output_request_queue_.pop();
    switch (request.type) {
      case OutputRequest::kFlushFence:
        DCHECK(output_request_queue_.empty());
        DVLOGF(2) << "Flush finished.";
        RunDecodeCB(std::move(flush_cb_), DecodeStatus::OK);
        resume_decode = true;
        break;

      case OutputRequest::kChangeResolutionFence:
        DCHECK(output_request_queue_.empty());
        if (!ChangeResolution()) {
          SetState(State::kError);
          return;
        }
        resume_decode = true;
        break;

      case OutputRequest::kSurface:
        scoped_refptr<V4L2DecodeSurface> surface = std::move(request.surface);

        DCHECK(surface->video_frame());
        RunOutputCB(surface->video_frame(), surface->visible_rect(),
                    request.timestamp);
        break;
    }
  }

  if (resume_decode) {
    SetState(State::kDecoding);
    decoder_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2SliceVideoDecoder::PumpDecodeTask, weak_this_));
  }
}

bool V4L2SliceVideoDecoder::ChangeResolution() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK_EQ(state_, State::kFlushing);
  // We change resolution after outputting all pending surfaces, there should
  // be no V4L2DecodeSurface left.
  DCHECK(surfaces_at_device_.empty());
  DCHECK_EQ(input_queue_->QueuedBuffersCount(), 0u);
  DCHECK_EQ(output_queue_->QueuedBuffersCount(), 0u);

  DCHECK(output_request_queue_.empty());
  if (!StopStreamV4L2Queue())
    return false;

  // Set output format with the new resolution.
  gfx::Size pic_size = avd_->GetPicSize();
  DCHECK(!pic_size.IsEmpty());
  DVLOGF(3) << "Change resolution to " << pic_size.width() << "x"
            << pic_size.height();
  auto frame_layout = SetupOutputFormat(pic_size, avd_->GetVisibleRect());
  if (!frame_layout) {
    VLOGF(1) << "No format is available with thew new resolution";
    return false;
  }

  auto coded_size = frame_layout->coded_size();
  DCHECK_EQ(coded_size.width() % 16, 0);
  DCHECK_EQ(coded_size.height() % 16, 0);
  if (!gfx::Rect(coded_size).Contains(gfx::Rect(pic_size))) {
    VLOGF(1) << "Got invalid adjusted coded size: " << coded_size.ToString();
    return false;
  }

  // Allocate new output buffers.
  if (!output_queue_->DeallocateBuffers())
    return false;
  size_t num_output_frames = avd_->GetRequiredNumOfPictures();
  DCHECK_GT(num_output_frames, 0u);
  if (output_queue_->AllocateBuffers(num_output_frames, V4L2_MEMORY_DMABUF) ==
      0) {
    VLOGF(1) << "Failed to request output buffers.";
    return false;
  }
  if (output_queue_->AllocatedBuffersCount() != num_output_frames) {
    VLOGF(1) << "Could not allocate requested number of output buffers.";
    return false;
  }
  frame_pool_->SetMaxNumFrames(num_output_frames);

  if (!StartStreamV4L2Queue())
    return false;

  SetState(State::kDecoding);
  return true;
}

scoped_refptr<V4L2DecodeSurface> V4L2SliceVideoDecoder::CreateSurface() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);

  // Request VideoFrame.
  scoped_refptr<VideoFrame> frame = frame_pool_->GetFrame();
  if (!frame) {
    // We allocate the same number of output buffer slot in V4L2 device and the
    // output VideoFrame. If there is free output buffer slot but no free
    // VideoFrame, surface_it means the VideoFrame is not released at client
    // side. Post PumpDecodeTask when the pool has available frames.
    DVLOGF(3) << "There is no available VideoFrame.";
    frame_pool_->NotifyWhenFrameAvailable(base::BindOnce(
        base::IgnoreResult(&base::SequencedTaskRunner::PostTask),
        decoder_task_runner_, FROM_HERE,
        base::BindOnce(&V4L2SliceVideoDecoder::PumpDecodeTask, weak_this_)));
    return nullptr;
  }
  frame->set_timestamp(current_decode_request_->buffer->timestamp());

  // Request V4L2 input and output buffers.
  V4L2WritableBufferRef input_buf = input_queue_->GetFreeBuffer();
  V4L2WritableBufferRef output_buf = output_queue_->GetFreeBuffer();
  if (!input_buf.IsValid() || !output_buf.IsValid()) {
    DVLOGF(3) << "There is no free V4L2 buffer.";
    return nullptr;
  }

  return scoped_refptr<V4L2DecodeSurface>(new V4L2ConfigStoreDecodeSurface(
      std::move(input_buf), std::move(output_buf), std::move(frame),
      base::DoNothing()));
}

void V4L2SliceVideoDecoder::ReuseOutputBuffer(V4L2ReadableBufferRef buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3) << "Reuse output surface #" << buffer->BufferId();

  // Resume decoding in case of ran out of surface.
  if (pause_reason_ == PauseReason::kRanOutOfSurfaces) {
    decoder_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2SliceVideoDecoder::PumpDecodeTask, weak_this_));
  }
}

bool V4L2SliceVideoDecoder::SubmitSlice(
    const scoped_refptr<V4L2DecodeSurface>& dec_surface,
    const uint8_t* data,
    size_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  size_t plane_size = dec_surface->input_buffer().GetPlaneSize(0);
  size_t bytes_used = dec_surface->input_buffer().GetPlaneBytesUsed(0);
  if (size > plane_size - bytes_used) {
    VLOGF(1) << "The size of submitted slice(" << size
             << ") is larger than the remaining buffer size("
             << plane_size - bytes_used << "). Plane size is " << plane_size;
    SetState(State::kError);
    return false;
  }

  void* mapping = dec_surface->input_buffer().GetPlaneMapping(0);
  memcpy(reinterpret_cast<uint8_t*>(mapping) + bytes_used, data, size);
  dec_surface->input_buffer().SetPlaneBytesUsed(0, bytes_used + size);
  return true;
}

void V4L2SliceVideoDecoder::DecodeSurface(
    const scoped_refptr<V4L2DecodeSurface>& dec_surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  // Enqueue input_buf and output_buf
  dec_surface->input_buffer().PrepareQueueBuffer(dec_surface);
  if (!std::move(dec_surface->input_buffer()).QueueMMap()) {
    SetState(State::kError);
    return;
  }

  if (!IsValidFrameForQueueDMABuf(dec_surface->video_frame().get(),
                                  num_output_planes_)) {
    SetState(State::kError);
    return;
  }
  if (!std::move(dec_surface->output_buffer())
           .QueueDMABuf(dec_surface->video_frame()->DmabufFds())) {
    SetState(State::kError);
    return;
  }

  if (!dec_surface->Submit()) {
    VLOGF(1) << "Error while submitting frame for decoding!";
    SetState(State::kError);
    return;
  }

  surfaces_at_device_.push(std::move(dec_surface));

  SchedulePollTaskIfNeeded();
}

void V4L2SliceVideoDecoder::SurfaceReady(
    const scoped_refptr<V4L2DecodeSurface>& dec_surface,
    int32_t bitstream_id,
    const gfx::Rect& visible_rect,
    const VideoColorSpace& /* color_space */) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  auto it = bitstream_id_to_timestamp_.find(bitstream_id);
  DCHECK(it != bitstream_id_to_timestamp_.end());
  base::TimeDelta timestamp = it->second;
  bitstream_id_to_timestamp_.erase(it);

  dec_surface->SetVisibleRect(visible_rect);
  output_request_queue_.push(
      OutputRequest::Surface(std::move(dec_surface), timestamp));
  PumpOutputSurfaces();
}

bool V4L2SliceVideoDecoder::StartStreamV4L2Queue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  if (!device_poll_thread_.IsRunning()) {
    if (!device_poll_thread_.Start()) {
      VLOGF(1) << "Failed to start device poll thread.";
      SetState(State::kError);
      return false;
    }
  }

  if (!input_queue_->Streamon() || !output_queue_->Streamon()) {
    VLOGF(1) << "Failed to streamon V4L2 queue.";
    SetState(State::kError);
    return false;
  }

  SchedulePollTaskIfNeeded();
  return true;
}

bool V4L2SliceVideoDecoder::StopStreamV4L2Queue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK_NE(state_, State::kUninitialized);
  DVLOGF(3);

  if (!device_poll_thread_.IsRunning())
    return true;

  if (!device_->SetDevicePollInterrupt()) {
    VLOGF(1) << "Failed to interrupt device poll.";
    SetState(State::kError);
    return false;
  }

  DVLOGF(3) << "Stop device poll thead";
  device_poll_thread_.Stop();
  if (!device_->ClearDevicePollInterrupt()) {
    VLOGF(1) << "Failed to clear interrupting device poll.";
    SetState(State::kError);
    return false;
  }

  // Streamoff input queue.
  if (input_queue_->IsStreaming())
    input_queue_->Streamoff();

  // Streamoff output queue.
  if (output_queue_->IsStreaming())
    output_queue_->Streamoff();
  while (!surfaces_at_device_.empty())
    surfaces_at_device_.pop();

  return true;
}

// Poke when we want to dequeue buffer from V4L2 device
void V4L2SliceVideoDecoder::SchedulePollTaskIfNeeded() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(input_queue_->IsStreaming() && output_queue_->IsStreaming());

  if (!device_poll_thread_.IsRunning()) {
    DVLOGF(4) << "Device poll thread stopped, will not schedule poll";
    return;
  }

  if (input_queue_->QueuedBuffersCount() == 0 &&
      output_queue_->QueuedBuffersCount() == 0) {
    DVLOGF(4) << "No buffers queued, will not schedule poll";
    return;
  }

  device_poll_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2SliceVideoDecoder::DevicePollTask,
                                base::Unretained(this)));
}

void V4L2SliceVideoDecoder::DevicePollTask() {
  DCHECK(device_poll_thread_.task_runner()->RunsTasksInCurrentSequence());
  DVLOGF(3);

  bool event_pending;
  if (!device_->Poll(true, &event_pending)) {
    decoder_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&V4L2SliceVideoDecoder::SetState, weak_this_,
                                  State::kError));
    return;
  }

  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V4L2SliceVideoDecoder::ServiceDeviceTask, weak_this_));
}

void V4L2SliceVideoDecoder::ServiceDeviceTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3) << "Number of queued input buffers: "
            << input_queue_->QueuedBuffersCount()
            << ", Number of queued output buffers: "
            << output_queue_->QueuedBuffersCount();

  bool resume_decode = false;
  // Dequeue V4L2 output buffer first to reduce output latency.
  bool success;
  V4L2ReadableBufferRef dequeued_buffer;
  while (output_queue_->QueuedBuffersCount() > 0) {
    std::tie(success, dequeued_buffer) = output_queue_->DequeueBuffer();
    if (!success) {
      SetState(State::kError);
      return;
    }
    if (!dequeued_buffer)
      break;

    // Mark the output buffer decoded, and try to output surface.
    DCHECK(!surfaces_at_device_.empty());
    auto surface = std::move(surfaces_at_device_.front());
    DCHECK_EQ(static_cast<size_t>(surface->output_record()),
              dequeued_buffer->BufferId());
    surfaces_at_device_.pop();

    surface->SetDecoded();
    // VP9Decoder update context after surface is decoded. Resume decoding for
    // previous pause of AVD::kWaitSubFrameDecoded.
    resume_decode = true;

    // Keep a reference to the V4L2 buffer until the buffer is reused. The
    // reason for this is that the config store uses V4L2 buffer IDs to
    // reference frames, therefore we cannot reuse the same V4L2 buffer ID for
    // another decode operation until all references to that frame are gone.
    // Request API does not have this limitation, so we can probably remove this
    // after config store is gone.
    surface->SetReleaseCallback(
        base::BindOnce(&V4L2SliceVideoDecoder::ReuseOutputBuffer, weak_this_,
                       std::move(dequeued_buffer)));

    PumpOutputSurfaces();
  }

  // Dequeue V4L2 input buffer.
  while (input_queue_->QueuedBuffersCount() > 0) {
    std::tie(success, dequeued_buffer) = input_queue_->DequeueBuffer();
    if (!success) {
      SetState(State::kError);
      return;
    }
    if (!dequeued_buffer)
      break;
  }

  SchedulePollTaskIfNeeded();

  if (resume_decode && pause_reason_ == PauseReason::kWaitSubFrameDecoded) {
    decoder_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2SliceVideoDecoder::PumpDecodeTask, weak_this_));
  }
}

int32_t V4L2SliceVideoDecoder::GetNextBitstreamId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & 0x7FFFFFFF;
  return next_bitstream_buffer_id_;
}

void V4L2SliceVideoDecoder::RunDecodeCB(DecodeCB cb, DecodeStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  client_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(std::move(cb), status));
}

void V4L2SliceVideoDecoder::RunOutputCB(scoped_refptr<VideoFrame> frame,
                                        const gfx::Rect& visible_rect,
                                        base::TimeDelta timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4) << "timestamp: " << timestamp;

  // We need to update one or more attributes of the frame. Since we can't
  // modify the attributes of the frame directly, we wrap the frame into a new
  // frame with updated attributes. The old frame is bound to a destruction
  // observer so it's not destroyed before the wrapped frame.
  if (frame->visible_rect() != visible_rect ||
      frame->timestamp() != timestamp) {
    gfx::Size natural_size = GetNaturalSize(visible_rect, pixel_aspect_ratio_);
    scoped_refptr<VideoFrame> wrapped_frame = VideoFrame::WrapVideoFrame(
        *frame, frame->format(), visible_rect, natural_size);
    wrapped_frame->set_timestamp(timestamp);
    wrapped_frame->AddDestructionObserver(base::BindOnce(
        base::DoNothing::Once<scoped_refptr<VideoFrame>>(), std::move(frame)));

    frame = std::move(wrapped_frame);
  }
  frame->metadata()->SetBoolean(VideoFrameMetadata::POWER_EFFICIENT, true);

  // Although the document of VideoDecoder says "should run |output_cb| as soon
  // as possible (without thread trampolining)", MojoVideoDecoderService still
  // assumes the callback is called at original thread.
  // TODO(akahuang): call the callback directly after updating MojoVDService.
  client_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(output_cb_, std::move(frame)));
}

void V4L2SliceVideoDecoder::SetState(State new_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3) << "Change state from " << static_cast<int>(state_) << " to "
            << static_cast<int>(new_state);

  if (state_ == new_state)
    return;
  if (state_ == State::kError) {
    DVLOGF(3) << "Already in kError state.";
    return;
  }

  // Check if the state transition is valid.
  switch (new_state) {
    case State::kUninitialized:
      if (state_ != State::kDecoding) {
        VLOGF(1) << "Should not set to kUninitialized.";
        new_state = State::kError;
      }
      break;

    case State::kDecoding:
      break;

    case State::kFlushing:
      if (state_ != State::kDecoding) {
        VLOGF(1) << "kFlushing should only be set when kDecoding.";
        new_state = State::kError;
      }
      break;

    case State::kError:
      break;
  }

  if (new_state == State::kError) {
    VLOGF(1) << "Error occurred.";
    ClearPendingRequests(DecodeStatus::DECODE_ERROR);
    return;
  }
  state_ = new_state;
  return;
}

}  // namespace media
