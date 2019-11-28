// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_image_processor.h"

#include <stdint.h>

#include <va/va.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
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

void ReportToUMA(scoped_refptr<base::SequencedTaskRunner> task_runner,
                 base::RepeatingClosure error_cb,
                 VaIPFailure failure) {
  base::UmaHistogramEnumeration("Media.VAIP.VppFailure", failure);
  task_runner->PostTask(FROM_HERE, error_cb);
}

bool IsSupported(uint32_t input_va_fourcc,
                 uint32_t output_va_fourcc,
                 const gfx::Size& input_size,
                 const gfx::Size& output_size) {
  if (!VaapiWrapper::IsVppFormatSupported(input_va_fourcc)) {
    VLOGF(2) << "Unsupported input format: VA_FOURCC_"
             << FourccToString(input_va_fourcc);
    return false;
  }

  if (!VaapiWrapper::IsVppFormatSupported(output_va_fourcc)) {
    VLOGF(2) << "Unsupported output format: VA_FOURCC_"
             << FourccToString(output_va_fourcc);
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

}  // namespace

// static
std::unique_ptr<VaapiImageProcessor> VaapiImageProcessor::Create(
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    const std::vector<ImageProcessor::OutputMode>& preferred_output_modes,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    const base::RepeatingClosure& error_cb) {
// VaapiImageProcessor supports ChromeOS only.
#if !defined(OS_CHROMEOS)
  return nullptr;
#endif

  if (!IsSupported(input_config.fourcc.ToVAFourCC(),
                   output_config.fourcc.ToVAFourCC(), input_config.size,
                   output_config.size)) {
    return nullptr;
  }

  if (!base::Contains(input_config.preferred_storage_types,
                      VideoFrame::STORAGE_DMABUFS) &&
      !base::Contains(input_config.preferred_storage_types,
                      VideoFrame::STORAGE_GPU_MEMORY_BUFFER)) {
    VLOGF(2) << "VaapiImageProcessor supports Dmabuf-backed or GpuMemoryBuffer"
             << " based VideoFrame only for input";
    return nullptr;
  }
  if (!base::Contains(output_config.preferred_storage_types,
                      VideoFrame::STORAGE_DMABUFS) &&
      !base::Contains(output_config.preferred_storage_types,
                      VideoFrame::STORAGE_GPU_MEMORY_BUFFER)) {
    VLOGF(2) << "VaapiImageProcessor supports Dmabuf-backed or GpuMemoryBuffer"
             << " based VideoFrame only for output";
    return nullptr;
  }

  if (!base::Contains(preferred_output_modes, OutputMode::IMPORT)) {
    VLOGF(2) << "VaapiImageProcessor only supports IMPORT mode.";
    return nullptr;
  }

  auto vaapi_wrapper = VaapiWrapper::Create(
      VaapiWrapper::kVideoProcess, VAProfileNone,
      base::BindRepeating(&ReportToUMA, client_task_runner, error_cb,
                          VaIPFailure::kVaapiVppError));
  if (!vaapi_wrapper) {
    VLOGF(1) << "Failed to create VaapiWrapper";
    return nullptr;
  }

  // Size is irrelevant for a VPP context.
  if (!vaapi_wrapper->CreateContext(gfx::Size())) {
    VLOGF(1) << "Failed to create context for VPP";
    return nullptr;
  }

  // We should restrict the acceptable PortConfig for input and output both to
  // the one returned by GetPlatformVideoFrameLayout(). However,
  // ImageProcessorFactory interface doesn't provide information about what
  // ImageProcessor will be used for. (e.g. format conversion after decoding and
  // scaling before encoding). Thus we cannot execute
  // GetPlatformVideoFrameLayout() with a proper gfx::BufferUsage.
  // TODO(crbug.com/898423): Adjust layout once ImageProcessor provide the use
  // scenario.
  return base::WrapUnique(new VaapiImageProcessor(
      input_config, output_config, std::move(vaapi_wrapper),
      std::move(client_task_runner)));
}

VaapiImageProcessor::VaapiImageProcessor(
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner)
    : ImageProcessor(input_config,
                     output_config,
                     OutputMode::IMPORT,
                     std::move(client_task_runner)),
      processor_task_runner_(base::CreateSequencedTaskRunner(
          base::TaskTraits{base::ThreadPool()})),
      vaapi_wrapper_(std::move(vaapi_wrapper)) {
  DETACH_FROM_SEQUENCE(client_sequence_checker_);
  DETACH_FROM_SEQUENCE(processor_sequence_checker_);
}

VaapiImageProcessor::~VaapiImageProcessor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
}

bool VaapiImageProcessor::ProcessInternal(
    scoped_refptr<VideoFrame> input_frame,
    scoped_refptr<VideoFrame> output_frame,
    FrameReadyCB cb) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  // TODO(akahuang): Use WeakPtr to replace base::Unretained(this).
  process_task_tracker_.PostTask(
      processor_task_runner_.get(), FROM_HERE,
      base::BindOnce(&VaapiImageProcessor::ProcessTask, base::Unretained(this),
                     std::move(input_frame), std::move(output_frame),
                     std::move(cb)));
  return true;
}

void VaapiImageProcessor::ProcessTask(scoped_refptr<VideoFrame> input_frame,
                                      scoped_refptr<VideoFrame> output_frame,
                                      FrameReadyCB cb) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(processor_sequence_checker_);

  auto src_va_surface =
      vaapi_wrapper_->CreateVASurfaceForVideoFrame(input_frame.get());
  auto dst_va_surface =
      vaapi_wrapper_->CreateVASurfaceForVideoFrame(output_frame.get());
  if (!src_va_surface || !dst_va_surface) {
    // Failed to create VASurface for frames. |cb| isn't executed in the case.
    return;
  }
  // VA-API performs pixel format conversion and scaling without any filters.
  vaapi_wrapper_->BlitSurface(std::move(src_va_surface),
                              std::move(dst_va_surface));
  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(cb), std::move(output_frame)));
}

bool VaapiImageProcessor::Reset() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  process_task_tracker_.TryCancelAll();
  return true;
}
}  // namespace media
