// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/gav1_video_decoder.h"

#include <stdint.h>
#include <numeric>

#include "base/bind_helpers.h"
#include "base/bits.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"
#include "media/filters/frame_buffer_pool.h"
#include "third_party/libgav1/src/src/gav1/decoder.h"
#include "third_party/libgav1/src/src/gav1/decoder_settings.h"
#include "third_party/libgav1/src/src/gav1/frame_buffer.h"

namespace media {

namespace {

VideoPixelFormat Libgav1ImageFormatToVideoPixelFormat(
    const libgav1::ImageFormat libgav1_format,
    int bitdepth) {
  switch (libgav1_format) {
    case libgav1::kImageFormatYuv420:
      switch (bitdepth) {
        case 8:
          return PIXEL_FORMAT_I420;
        case 10:
          return PIXEL_FORMAT_YUV420P10;
        case 12:
          return PIXEL_FORMAT_YUV420P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << bitdepth;
          return PIXEL_FORMAT_UNKNOWN;
      }
    case libgav1::kImageFormatYuv422:
      switch (bitdepth) {
        case 8:
          return PIXEL_FORMAT_I422;
        case 10:
          return PIXEL_FORMAT_YUV422P10;
        case 12:
          return PIXEL_FORMAT_YUV422P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << bitdepth;
          return PIXEL_FORMAT_UNKNOWN;
      }
    case libgav1::kImageFormatYuv444:
      switch (bitdepth) {
        case 8:
          return PIXEL_FORMAT_I444;
        case 10:
          return PIXEL_FORMAT_YUV444P10;
        case 12:
          return PIXEL_FORMAT_YUV444P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << bitdepth;
          return PIXEL_FORMAT_UNKNOWN;
      }
    default:
      DLOG(ERROR) << "Unsupported pixel format: " << libgav1_format;
      return PIXEL_FORMAT_UNKNOWN;
  }
}

int GetDecoderThreadCounts(int coded_height) {
  // Tile thread counts based on currently available content. Recommended by
  // YouTube, while frame thread values fit within limits::kMaxVideoThreads.
  // libgav1 doesn't support parallel frame decoding.
  // We can set the number of tile threads to as many as we like, but not
  // greater than limits::kMaxVideoDecodeThreads.
  static const int num_cores = base::SysInfo::NumberOfProcessors();
  auto threads_by_height = [](int coded_height) {
    if (coded_height >= 1000)
      return 8;
    if (coded_height >= 700)
      return 5;
    if (coded_height >= 300)
      return 3;
    return 2;
  };

  return std::min(threads_by_height(coded_height), num_cores);
}

int GetFrameBufferImpl(void* private_data,
                       size_t y_plane_min_size,
                       size_t uv_plane_min_size,
                       Libgav1FrameBuffer* frame_buffer) {
  DCHECK(private_data);
  DCHECK(frame_buffer);
  auto* pool = reinterpret_cast<FrameBufferPool*>(private_data);
  const size_t sizes[] = {
      y_plane_min_size,
      uv_plane_min_size,
      uv_plane_min_size,
  };
  const size_t buffer_size = std::accumulate(
      std::cbegin(sizes), std::cend(sizes), static_cast<size_t>(0u));
  uint8_t* buf = pool->GetFrameBuffer(buffer_size, &frame_buffer->private_data);
  for (size_t i = 0; i < base::size(sizes); ++i) {
    size_t sz = sizes[i];
    frame_buffer->data[i] = sz > 0 ? buf : nullptr;
    frame_buffer->size[i] = sz;
    buf += sz;
  }

  // Return 0 on success.
  return 0;
}

int ReleaseFrameBufferImpl(void* private_data,
                           Libgav1FrameBuffer* frame_buffer) {
  DCHECK(private_data);
  DCHECK(frame_buffer);
  if (!frame_buffer->private_data)
    return -1;
  auto* pool = reinterpret_cast<FrameBufferPool*>(private_data);
  pool->ReleaseFrameBuffer(frame_buffer->private_data);

  // Return 0 on success.
  return 0;
}

scoped_refptr<VideoFrame> FormatVideoFrame(
    const libgav1::DecoderBuffer& buffer,
    const gfx::Size& natural_size,
    const VideoColorSpace& container_color_space,
    FrameBufferPool* memory_pool) {
  gfx::Size coded_size(buffer.stride[0], buffer.displayed_height[0]);
  gfx::Rect visible_rect(buffer.displayed_width[0], buffer.displayed_height[0]);

  auto frame = VideoFrame::WrapExternalYuvData(
      Libgav1ImageFormatToVideoPixelFormat(buffer.image_format,
                                           buffer.bitdepth),
      coded_size, visible_rect, natural_size, buffer.stride[0],
      buffer.stride[1], buffer.stride[2], buffer.plane[0], buffer.plane[1],
      buffer.plane[2],
      base::TimeDelta::FromMicroseconds(buffer.user_private_data));

  // AV1 color space defines match ISO 23001-8:2016 via ISO/IEC 23091-4/ITU-T
  // H.273. https://aomediacodec.github.io/av1-spec/#color-config-semantics
  media::VideoColorSpace color_space(
      buffer.color_primary, buffer.transfer_characteristics,
      buffer.matrix_coefficients,
      buffer.color_range == libgav1::kColorRangeStudio
          ? gfx::ColorSpace::RangeID::LIMITED
          : gfx::ColorSpace::RangeID::FULL);

  // If the frame doesn't specify a color space, use the container's.
  if (!color_space.IsSpecified())
    color_space = container_color_space;

  frame->set_color_space(color_space.ToGfxColorSpace());
  frame->metadata()->SetBoolean(VideoFrameMetadata::POWER_EFFICIENT, false);

  // Ensure the frame memory is returned to the MemoryPool upon discard.
  frame->AddDestructionObserver(
      memory_pool->CreateFrameCallback(buffer.buffer_private_data));
  return frame;
}

}  // namespace

Gav1VideoDecoder::DecodeRequest::DecodeRequest(
    scoped_refptr<DecoderBuffer> buffer,
    DecodeCB decode_cb)
    : buffer(std::move(buffer)), decode_cb(std::move(decode_cb)) {}

Gav1VideoDecoder::DecodeRequest::~DecodeRequest() {
  if (decode_cb)
    std::move(decode_cb).Run(DecodeStatus::ABORTED);
}

Gav1VideoDecoder::DecodeRequest::DecodeRequest(DecodeRequest&& other)
    : buffer(std::move(other.buffer)), decode_cb(std::move(other.decode_cb)) {}

Gav1VideoDecoder::Gav1VideoDecoder(MediaLog* media_log,
                                   OffloadState offload_state)
    : media_log_(media_log),
      bind_callbacks_(offload_state == OffloadState::kNormal) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Gav1VideoDecoder::~Gav1VideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CloseDecoder();
}

std::string Gav1VideoDecoder::GetDisplayName() const {
  return "Gav1VideoDecoder";
}

int Gav1VideoDecoder::GetMaxDecodeRequests() const {
  DCHECK(libgav1_decoder_);
  return libgav1_decoder_->GetMaxAllowedFrames();
}

void Gav1VideoDecoder::Initialize(const VideoDecoderConfig& config,
                                  bool low_delay,
                                  CdmContext* /* cdm_context */,
                                  InitCB init_cb,
                                  const OutputCB& output_cb,
                                  const WaitingCB& /* waiting_cb */) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config.IsValidConfig());

  InitCB bound_init_cb = bind_callbacks_ ? BindToCurrentLoop(std::move(init_cb))
                                         : std::move(init_cb);
  if (config.is_encrypted() || config.codec() != kCodecAV1) {
    std::move(bound_init_cb).Run(false);
    return;
  }

  // Clear any previously initialized decoder.
  CloseDecoder();

  DCHECK(!memory_pool_);
  memory_pool_ = new FrameBufferPool();

  libgav1::DecoderSettings settings;
  settings.threads = VideoDecoder::GetRecommendedThreadCount(
      GetDecoderThreadCounts(config.coded_size().height()));
  settings.get = GetFrameBufferImpl;
  settings.release = ReleaseFrameBufferImpl;
  settings.callback_private_data = memory_pool_.get();

  libgav1_decoder_ = std::make_unique<libgav1::Decoder>();
  libgav1::StatusCode status = libgav1_decoder_->Init(&settings);
  if (status != kLibgav1StatusOk) {
    MEDIA_LOG(ERROR, media_log_) << "libgav1::Decoder::Init() failed, "
                                 << "status=" << status;
    std::move(bound_init_cb).Run(false);
    return;
  }

  output_cb_ = output_cb;
  state_ = DecoderState::kDecoding;
  color_space_ = config.color_space_info();
  natural_size_ = config.natural_size();
  std::move(bound_init_cb).Run(true);
}

void Gav1VideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                              DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer);
  DCHECK(decode_cb);
  DCHECK(libgav1_decoder_);
  DCHECK_NE(state_, DecoderState::kUninitialized)
      << "Called Decode() before successful Initilize()";

  DecodeCB bound_decode_cb = bind_callbacks_
                                 ? BindToCurrentLoop(std::move(decode_cb))
                                 : std::move(decode_cb);

  if (state_ == DecoderState::kError) {
    DCHECK(decode_queue_.empty());
    std::move(bound_decode_cb).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  if (!EnqueueRequest(
          DecodeRequest(std::move(buffer), std::move(bound_decode_cb)))) {
    SetError();
    DLOG(ERROR) << "Enqueue failed";
    return;
  }

  if (!MaybeDequeueFrames()) {
    SetError();
    DLOG(ERROR) << "Dequeue failed";
    return;
  }
}

void Gav1VideoDecoder::Reset(base::OnceClosure reset_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = DecoderState::kDecoding;

  libgav1::StatusCode status = libgav1_decoder_->SignalEOS();
  // This will invoke decode_cb with DecodeStatus::ABORTED.
  decode_queue_ = {};
  if (status != kLibgav1StatusOk) {
    MEDIA_LOG(WARNING, media_log_) << "libgav1::Decoder::SingleEOS() failed, "
                                   << "status=" << status;
  }

  if (bind_callbacks_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(reset_cb));
  } else {
    std::move(reset_cb).Run();
  }
}

void Gav1VideoDecoder::Detach() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!bind_callbacks_);

  CloseDecoder();

  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void Gav1VideoDecoder::CloseDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  libgav1_decoder_.reset();
  state_ = DecoderState::kUninitialized;

  if (memory_pool_) {
    memory_pool_->Shutdown();
    memory_pool_ = nullptr;
  }

  decode_queue_ = {};
}

void Gav1VideoDecoder::SetError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = DecoderState::kError;
  while (!decode_queue_.empty()) {
    DecodeRequest request = std::move(decode_queue_.front());
    std::move(request.decode_cb).Run(DecodeStatus::DECODE_ERROR);
    decode_queue_.pop();
  }
}

bool Gav1VideoDecoder::EnqueueRequest(DecodeRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const DecoderBuffer* buffer = request.buffer.get();
  decode_queue_.push(std::move(request));

  if (buffer->end_of_stream())
    return true;

  libgav1::StatusCode status = libgav1_decoder_->EnqueueFrame(
      buffer->data(), buffer->data_size(),
      buffer->timestamp().InMicroseconds() /* user_private_data */);
  if (status != kLibgav1StatusOk) {
    MEDIA_LOG(ERROR, media_log_)
        << "libgav1::Decoder::EnqueueFrame() failed, "
        << "status=" << status << " on " << buffer->AsHumanReadableString();
    return false;
  }
  return true;
}

bool Gav1VideoDecoder::MaybeDequeueFrames() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (true) {
    const libgav1::DecoderBuffer* buffer;
    libgav1::StatusCode status = libgav1_decoder_->DequeueFrame(&buffer);
    if (status != kLibgav1StatusOk) {
      MEDIA_LOG(ERROR, media_log_) << "libgav1::Decoder::DequeueFrame failed, "
                                   << "status=" << status;
      return false;
    }

    if (!buffer) {
      // This is not an error case; no displayable buffer exists or is ready.
      break;
    }

    // Check if decoding is done in FIFO manner.
    DecodeRequest request = std::move(decode_queue_.front());
    decode_queue_.pop();
    if (request.buffer->timestamp() !=
        base::TimeDelta::FromMicroseconds(buffer->user_private_data)) {
      MEDIA_LOG(ERROR, media_log_) << "Doesn't decode in FIFO manner on "
                                   << request.buffer->AsHumanReadableString();
      return false;
    }

    scoped_refptr<VideoFrame> frame = FormatVideoFrame(
        *buffer, natural_size_, color_space_, memory_pool_.get());
    if (!frame) {
      MEDIA_LOG(ERROR, media_log_) << "Failed formatting VideoFrame from "
                                   << "libgav1::DecoderBuffer";
      return false;
    }

    output_cb_.Run(std::move(frame));
    std::move(request.decode_cb).Run(DecodeStatus::OK);
  }

  // Execute |decode_cb| if the top of |decode_queue_| is EOS frame.
  if (!decode_queue_.empty() && decode_queue_.front().buffer->end_of_stream()) {
    std::move(decode_queue_.front().decode_cb).Run(DecodeStatus::OK);
    decode_queue_.pop();
  }

  return true;
}

}  // namespace media
