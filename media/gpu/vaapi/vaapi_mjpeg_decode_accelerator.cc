// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_mjpeg_decode_accelerator.h"

#include <stddef.h>
#include <va/va.h>

#include <utility>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/unaligned_shared_memory.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_image_decoder.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

// UMA errors that the VaapiMjpegDecodeAccelerator class reports.
enum VAJDADecoderFailure {
  VAAPI_ERROR = 0,
  VAJDA_DECODER_FAILURES_MAX,
};

static void ReportToVAJDADecoderFailureUMA(VAJDADecoderFailure failure) {
  UMA_HISTOGRAM_ENUMERATION("Media.VAJDA.DecoderFailure", failure,
                            VAJDA_DECODER_FAILURES_MAX + 1);
}

static void ReportToVAJDAResponseToClientUMA(
    chromeos_camera::MjpegDecodeAccelerator::Error response) {
  UMA_HISTOGRAM_ENUMERATION(
      "Media.VAJDA.ResponseToClient", response,
      chromeos_camera::MjpegDecodeAccelerator::Error::MJDA_ERROR_CODE_MAX + 1);
}

static chromeos_camera::MjpegDecodeAccelerator::Error
VaapiJpegDecodeStatusToError(VaapiImageDecodeStatus status) {
  switch (status) {
    case VaapiImageDecodeStatus::kSuccess:
      return chromeos_camera::MjpegDecodeAccelerator::Error::NO_ERRORS;
    case VaapiImageDecodeStatus::kParseFailed:
      return chromeos_camera::MjpegDecodeAccelerator::Error::PARSE_JPEG_FAILED;
    case VaapiImageDecodeStatus::kUnsupportedSubsampling:
      return chromeos_camera::MjpegDecodeAccelerator::Error::UNSUPPORTED_JPEG;
    default:
      return chromeos_camera::MjpegDecodeAccelerator::Error::PLATFORM_FAILURE;
  }
}

static bool VerifyDataSize(const VAImage* image) {
  const gfx::Size dimensions(base::strict_cast<int>(image->width),
                             base::strict_cast<int>(image->height));
  size_t min_size = 0;
  if (image->format.fourcc == VA_FOURCC_I420) {
    min_size = VideoFrame::AllocationSize(PIXEL_FORMAT_I420, dimensions);
  } else if (image->format.fourcc == VA_FOURCC_YUY2 ||
             image->format.fourcc == VA_FOURCC('Y', 'U', 'Y', 'V')) {
    min_size = VideoFrame::AllocationSize(PIXEL_FORMAT_YUY2, dimensions);
  } else {
    return false;
  }
  return base::strict_cast<size_t>(image->data_size) >= min_size;
}

}  // namespace

void VaapiMjpegDecodeAccelerator::NotifyError(int32_t bitstream_buffer_id,
                                              Error error) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiMjpegDecodeAccelerator::NotifyError,
                                  weak_this_factory_.GetWeakPtr(),
                                  bitstream_buffer_id, error));
    return;
  }
  VLOGF(1) << "Notifying of error " << error;
  // |error| shouldn't be NO_ERRORS because successful decodes should be handled
  // by VideoFrameReady().
  DCHECK_NE(chromeos_camera::MjpegDecodeAccelerator::Error::NO_ERRORS, error);
  ReportToVAJDAResponseToClientUMA(error);
  DCHECK(client_);
  client_->NotifyError(bitstream_buffer_id, error);
}

void VaapiMjpegDecodeAccelerator::VideoFrameReady(int32_t bitstream_buffer_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  ReportToVAJDAResponseToClientUMA(
      chromeos_camera::MjpegDecodeAccelerator::Error::NO_ERRORS);
  client_->VideoFrameReady(bitstream_buffer_id);
}

VaapiMjpegDecodeAccelerator::VaapiMjpegDecodeAccelerator(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(io_task_runner),
      client_(nullptr),
      decoder_thread_("VaapiMjpegDecoderThread"),
      weak_this_factory_(this) {}

VaapiMjpegDecodeAccelerator::~VaapiMjpegDecodeAccelerator() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  VLOGF(2) << "Destroying VaapiMjpegDecodeAccelerator";

  weak_this_factory_.InvalidateWeakPtrs();
  decoder_thread_.Stop();
}

bool VaapiMjpegDecodeAccelerator::Initialize(
    chromeos_camera::MjpegDecodeAccelerator::Client* client) {
  VLOGF(2);
  DCHECK(task_runner_->BelongsToCurrentThread());

  client_ = client;

  if (!decoder_.Initialize(
          base::BindRepeating(&ReportToVAJDADecoderFailureUMA, VAAPI_ERROR))) {
    return false;
  }

  if (!decoder_thread_.Start()) {
    VLOGF(1) << "Failed to start decoding thread.";
    return false;
  }
  decoder_task_runner_ = decoder_thread_.task_runner();

  return true;
}

bool VaapiMjpegDecodeAccelerator::OutputPictureOnTaskRunner(
    std::unique_ptr<ScopedVAImage> scoped_image,
    int32_t input_buffer_id,
    scoped_refptr<VideoFrame> video_frame) {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  TRACE_EVENT1("jpeg", "VaapiMjpegDecodeAccelerator::OutputPictureOnTaskRunner",
               "input_buffer_id", input_buffer_id);

  // Copy image content from VAImage to VideoFrame. If the image is not in the
  // I420 format we'll have to convert it.
  DCHECK(scoped_image);
  auto* mem = static_cast<uint8_t*>(scoped_image->va_buffer()->data());
  const VAImage* image = scoped_image->image();
  DCHECK_GE(base::strict_cast<int>(image->width),
            video_frame->coded_size().width());
  DCHECK_GE(base::strict_cast<int>(image->height),
            video_frame->coded_size().height());
  DCHECK(VerifyDataSize(image));
  uint8_t* dst_y = video_frame->data(VideoFrame::kYPlane);
  uint8_t* dst_u = video_frame->data(VideoFrame::kUPlane);
  uint8_t* dst_v = video_frame->data(VideoFrame::kVPlane);
  size_t dst_y_stride = video_frame->stride(VideoFrame::kYPlane);
  size_t dst_u_stride = video_frame->stride(VideoFrame::kUPlane);
  size_t dst_v_stride = video_frame->stride(VideoFrame::kVPlane);

  switch (image->format.fourcc) {
    case VA_FOURCC_I420: {
      DCHECK_EQ(image->num_planes, 3u);
      const uint8_t* src_y = mem + image->offsets[0];
      const uint8_t* src_u = mem + image->offsets[1];
      const uint8_t* src_v = mem + image->offsets[2];
      const size_t src_y_stride = image->pitches[0];
      const size_t src_u_stride = image->pitches[1];
      const size_t src_v_stride = image->pitches[2];
      if (libyuv::I420Copy(src_y, src_y_stride, src_u, src_u_stride, src_v,
                           src_v_stride, dst_y, dst_y_stride, dst_u,
                           dst_u_stride, dst_v, dst_v_stride,
                           video_frame->coded_size().width(),
                           video_frame->coded_size().height())) {
        VLOGF(1) << "I420Copy failed";
        return false;
      }
      break;
    }
    case VA_FOURCC_YUY2:
    case VA_FOURCC('Y', 'U', 'Y', 'V'): {
      DCHECK_EQ(image->num_planes, 1u);
      const uint8_t* src_yuy2 = mem + image->offsets[0];
      const size_t src_yuy2_stride = image->pitches[0];
      if (libyuv::YUY2ToI420(src_yuy2, src_yuy2_stride, dst_y, dst_y_stride,
                             dst_u, dst_u_stride, dst_v, dst_v_stride,
                             video_frame->coded_size().width(),
                             video_frame->coded_size().height())) {
        VLOGF(1) << "YUY2ToI420 failed";
        return false;
      }
      break;
    }
    default:
      VLOGF(1) << "Can't convert image to I420: unsupported format "
               << FourccToString(image->format.fourcc);
      return false;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiMjpegDecodeAccelerator::VideoFrameReady,
                     weak_this_factory_.GetWeakPtr(), input_buffer_id));

  return true;
}

void VaapiMjpegDecodeAccelerator::DecodeTask(
    int32_t bitstream_buffer_id,
    std::unique_ptr<UnalignedSharedMemory> shm,
    scoped_refptr<VideoFrame> video_frame) {
  DVLOGF(4);
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("jpeg", "DecodeTask");

  VaapiImageDecodeStatus status = decoder_.Decode(
      base::make_span(static_cast<const uint8_t*>(shm->memory()), shm->size()));
  if (status != VaapiImageDecodeStatus::kSuccess) {
    NotifyError(bitstream_buffer_id, VaapiJpegDecodeStatusToError(status));
    return;
  }
  std::unique_ptr<ScopedVAImage> image =
      decoder_.GetImage(VA_FOURCC_I420 /* preferred_image_fourcc */, &status);
  if (status != VaapiImageDecodeStatus::kSuccess) {
    NotifyError(bitstream_buffer_id, VaapiJpegDecodeStatusToError(status));
    return;
  }

  if (!OutputPictureOnTaskRunner(std::move(image), bitstream_buffer_id,
                                 std::move(video_frame))) {
    VLOGF(1) << "Output picture failed";
    NotifyError(bitstream_buffer_id, PLATFORM_FAILURE);
  }
}

void VaapiMjpegDecodeAccelerator::Decode(
    BitstreamBuffer bitstream_buffer,
    scoped_refptr<VideoFrame> video_frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("jpeg", "Decode", "input_id", bitstream_buffer.id());

  DVLOGF(4) << "Mapping new input buffer id: " << bitstream_buffer.id()
            << " size: " << bitstream_buffer.size();

  // UnalignedSharedMemory will take over the |bitstream_buffer.handle()|.
  auto shm = std::make_unique<UnalignedSharedMemory>(
      bitstream_buffer.TakeRegion(), bitstream_buffer.size(),
      false /* read_only */);

  if (bitstream_buffer.id() < 0) {
    VLOGF(1) << "Invalid bitstream_buffer, id: " << bitstream_buffer.id();
    NotifyError(bitstream_buffer.id(), INVALID_ARGUMENT);
    return;
  }

  if (!shm->MapAt(bitstream_buffer.offset(), bitstream_buffer.size())) {
    VLOGF(1) << "Failed to map input buffer";
    NotifyError(bitstream_buffer.id(), UNREADABLE_INPUT);
    return;
  }

  // It's safe to use base::Unretained(this) because |decoder_task_runner_| runs
  // tasks on |decoder_thread_| which is stopped in the destructor of |this|.
  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VaapiMjpegDecodeAccelerator::DecodeTask,
                                base::Unretained(this), bitstream_buffer.id(),
                                std::move(shm), std::move(video_frame)));
}

bool VaapiMjpegDecodeAccelerator::IsSupported() {
  return VaapiWrapper::IsDecodeSupported(VAProfileJPEGBaseline);
}

}  // namespace media
