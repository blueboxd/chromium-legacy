// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_image_processor.h"

#include <va/va.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/linux/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/native_pixmap.h"

namespace media {

namespace {
// UMA errors that the VaapiImageProcessor class reports.
enum class VaIPFailure {
  kVaapiVppError = 0,
  kMaxValue = kVaapiVppError,
};

void ReportToUMA(base::RepeatingClosure error_cb, VaIPFailure failure) {
  base::UmaHistogramEnumeration("Media.VAIP.VppFailure", failure);
  error_cb.Run();
}

bool IsSupported(VideoPixelFormat input_format,
                 VideoPixelFormat output_format,
                 gfx::Size input_size,
                 gfx::Size output_size) {
  const uint32_t input_va_fourcc =
      Fourcc::FromVideoPixelFormat(input_format).ToVAFourCC();
  if (!VaapiWrapper::IsVppFormatSupported(input_va_fourcc)) {
    VLOGF(2) << "Unsupported input format: " << input_format << " (VA_FOURCC_"
             << FourccToString(input_va_fourcc) << ")";
    return false;
  }

  const uint32_t output_va_fourcc =
      Fourcc::FromVideoPixelFormat(output_format).ToVAFourCC();
  if (!VaapiWrapper::IsVppFormatSupported(output_va_fourcc)) {
    VLOGF(2) << "Unsupported output format: " << output_format << " (VA_FOURCC_"
             << FourccToString(output_va_fourcc) << ")";
    return false;
  }

  if (!VaapiWrapper::IsVppResolutionAllowed(input_size)) {
    VLOGF(2) << "Unsupported input size: " << input_size.ToString();
    return false;
  }

  if (!VaapiWrapper::IsVppResolutionAllowed(output_size)) {
    VLOGF(2) << "Unsupported output size: " << output_size.ToString();
    return false;
  }

  return true;
}

void ProcessTask(scoped_refptr<VideoFrame> input_frame,
                 scoped_refptr<VideoFrame> output_frame,
                 ImageProcessor::FrameReadyCB cb,
                 scoped_refptr<VaapiWrapper> vaapi_wrapper) {
  DVLOGF(4);

  auto src_va_surface =
      vaapi_wrapper->CreateVASurfaceForVideoFrame(input_frame.get());
  auto dst_va_surface =
      vaapi_wrapper->CreateVASurfaceForVideoFrame(output_frame.get());
  if (!src_va_surface || !dst_va_surface) {
    // Failed to create VASurface for frames. |cb| isn't executed in the case.
    return;
  }
  // VA-API performs pixel format conversion and scaling without any filters.
  vaapi_wrapper->BlitSurface(std::move(src_va_surface),
                             std::move(dst_va_surface));
  std::move(cb).Run(std::move(output_frame));
}

}  // namespace

// static
std::unique_ptr<VaapiImageProcessor> VaapiImageProcessor::Create(
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    const std::vector<ImageProcessor::OutputMode>& preferred_output_modes,
    const base::RepeatingClosure& error_cb) {
// VaapiImageProcessor supports ChromeOS only.
#if !defined(OS_CHROMEOS)
  return nullptr;
#endif

  const VideoFrameLayout& input_layout = input_config.layout;
  const VideoFrameLayout& output_layout = output_config.layout;
  if (!IsSupported(input_layout.format(), output_layout.format(),
                   input_config.layout.coded_size(),
                   output_config.layout.coded_size())) {
    return nullptr;
  }

  if (!base::Contains(input_config.preferred_storage_types,
                      VideoFrame::STORAGE_DMABUFS) ||
      !base::Contains(output_config.preferred_storage_types,
                      VideoFrame::STORAGE_DMABUFS)) {
    VLOGF(2) << "VaapiImageProcessor supports Dmabuf-backed VideoFrame only "
             << "for both input and output";
    return nullptr;
  }

  if (!base::Contains(preferred_output_modes, OutputMode::IMPORT)) {
    VLOGF(2) << "VaapiImageProcessor only supports IMPORT mode.";
    return nullptr;
  }

  auto vaapi_wrapper = VaapiWrapper::Create(
      VaapiWrapper::kVideoProcess, VAProfileNone,
      base::BindRepeating(&ReportToUMA, error_cb, VaIPFailure::kVaapiVppError));
  if (!vaapi_wrapper) {
    VLOGF(1) << "Failed to create VaapiWrapper";
    return nullptr;
  }

  // We should restrict the acceptable VideoFrameLayout for input and output
  // both to the one returned by GetPlatformVideoFrameLayout(). However,
  // ImageProcessorFactory interface doesn't provide information about what
  // ImageProcessor will be used for. (e.g. format conversion after decoding and
  // scaling before encoding). Thus we cannot execute
  // GetPlatformVideoFrameLayout() with a proper gfx::BufferUsage.
  // TODO(crbug.com/898423): Adjust layout once ImageProcessor provide the use
  // scenario.
  return base::WrapUnique(new VaapiImageProcessor(input_layout, output_layout,
                                                  std::move(vaapi_wrapper)));
}

VaapiImageProcessor::VaapiImageProcessor(
    const VideoFrameLayout& input_layout,
    const VideoFrameLayout& output_layout,
    scoped_refptr<VaapiWrapper> vaapi_wrapper)
    : ImageProcessor(input_layout,
                     VideoFrame::STORAGE_DMABUFS,
                     output_layout,
                     VideoFrame::STORAGE_DMABUFS,
                     OutputMode::IMPORT),
      processor_task_runner_(base::CreateSequencedTaskRunner(
          base::TaskTraits{base::ThreadPool()})),
      vaapi_wrapper_(std::move(vaapi_wrapper)) {}

VaapiImageProcessor::~VaapiImageProcessor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
}

bool VaapiImageProcessor::ProcessInternal(
    scoped_refptr<VideoFrame> input_frame,
    scoped_refptr<VideoFrame> output_frame,
    FrameReadyCB cb) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(input_frame);
  DCHECK(output_frame);

  const VideoFrameLayout& input_frame_layout = input_frame->layout();
  if (input_frame_layout.format() != input_layout_.format() ||
      input_frame_layout.coded_size() != input_layout_.coded_size()) {
    VLOGF(1) << "Invalid input_frame->layout=" << input_frame->layout()
             << ", input_layout_=" << input_layout_;
    return false;
  }

  const VideoFrameLayout& output_frame_layout = output_frame->layout();
  if (output_frame_layout.format() != output_layout_.format() ||
      output_frame_layout.coded_size() != output_layout_.coded_size()) {
    VLOGF(1) << "Invalid output_frame->layout=" << output_frame->layout()
             << ", output_layout_=" << output_layout_;
    return false;
  }

  if (input_frame->storage_type() != input_storage_type()) {
    VLOGF(1) << "Invalid input_frame->storage_type="
             << input_frame->storage_type()
             << ", input_storage_type=" << input_storage_type();
    return false;
  }
  if (output_frame->storage_type() != output_storage_type()) {
    VLOGF(1) << "Invalid output_frame->storage_type="
             << output_frame->storage_type()
             << ", output_storage_type=" << output_storage_type();
    return false;
  }

  process_task_tracker_.PostTask(
      processor_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ProcessTask, std::move(input_frame),
                     std::move(output_frame), std::move(cb), vaapi_wrapper_));
  return true;
}

bool VaapiImageProcessor::Reset() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  process_task_tracker_.TryCancelAll();
  return true;
}
}  // namespace media
