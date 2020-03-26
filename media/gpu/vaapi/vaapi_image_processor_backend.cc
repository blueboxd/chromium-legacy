// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_image_processor_backend.h"

#include <stdint.h>

#include <va/va.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/native_pixmap.h"

namespace media {

namespace {
// UMA errors that the VaapiImageProcessorBackend class reports.
enum class VaIPFailure {
  kVaapiVppError = 0,
  kMaxValue = kVaapiVppError,
};

void ReportToUMA(base::RepeatingClosure error_cb, VaIPFailure failure) {
  base::UmaHistogramEnumeration("Media.VAIP.VppFailure", failure);
  error_cb.Run();
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
std::unique_ptr<ImageProcessorBackend> VaapiImageProcessorBackend::Create(
    const PortConfig& input_config,
    const PortConfig& output_config,
    const std::vector<OutputMode>& preferred_output_modes,
    ErrorCB error_cb,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner) {
// VaapiImageProcessorBackend supports ChromeOS only.
#if !defined(OS_CHROMEOS)
  return nullptr;
#endif

  auto input_vafourcc = input_config.fourcc.ToVAFourCC();
  if (!input_vafourcc) {
    VLOGF(2) << "Input fourcc " << input_config.fourcc.ToString()
             << " not compatible with VAAPI.";
    return nullptr;
  }

  auto output_vafourcc = output_config.fourcc.ToVAFourCC();
  if (!output_vafourcc) {
    VLOGF(2) << "Output fourcc " << output_config.fourcc.ToString()
             << " not compatible with VAAPI.";
    return nullptr;
  }

  if (!IsSupported(*input_vafourcc, *output_vafourcc, input_config.size,
                   output_config.size)) {
    return nullptr;
  }

  if (!base::Contains(input_config.preferred_storage_types,
                      VideoFrame::STORAGE_DMABUFS) &&
      !base::Contains(input_config.preferred_storage_types,
                      VideoFrame::STORAGE_GPU_MEMORY_BUFFER)) {
    VLOGF(2) << "VaapiImageProcessorBackend supports Dmabuf-backed or "
                "GpuMemoryBuffer based VideoFrame only for input";
    return nullptr;
  }
  if (!base::Contains(output_config.preferred_storage_types,
                      VideoFrame::STORAGE_DMABUFS) &&
      !base::Contains(output_config.preferred_storage_types,
                      VideoFrame::STORAGE_GPU_MEMORY_BUFFER)) {
    VLOGF(2) << "VaapiImageProcessorBackend supports Dmabuf-backed or "
                "GpuMemoryBuffer based VideoFrame only for output";
    return nullptr;
  }

  if (!base::Contains(preferred_output_modes, OutputMode::IMPORT)) {
    VLOGF(2) << "VaapiImageProcessorBackend only supports IMPORT mode.";
    return nullptr;
  }

  auto vaapi_wrapper = VaapiWrapper::Create(
      VaapiWrapper::kVideoProcess, VAProfileNone,
      base::BindRepeating(&ReportToUMA, error_cb, VaIPFailure::kVaapiVppError));
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
  return base::WrapUnique<ImageProcessorBackend>(new VaapiImageProcessorBackend(
      std::move(vaapi_wrapper), input_config, output_config, OutputMode::IMPORT,
      std::move(error_cb), std::move(backend_task_runner)));
}

VaapiImageProcessorBackend::VaapiImageProcessorBackend(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    const PortConfig& input_config,
    const PortConfig& output_config,
    OutputMode output_mode,
    ErrorCB error_cb,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : ImageProcessorBackend(input_config,
                            output_config,
                            output_mode,
                            std::move(error_cb),
                            std::move(backend_task_runner)),
      vaapi_wrapper_(std::move(vaapi_wrapper)) {}

VaapiImageProcessorBackend::~VaapiImageProcessorBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);
}

void VaapiImageProcessorBackend::Process(scoped_refptr<VideoFrame> input_frame,
                                         scoped_refptr<VideoFrame> output_frame,
                                         FrameReadyCB cb) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);

  DCHECK(input_frame);
  DCHECK(output_frame);
  scoped_refptr<gfx::NativePixmap> input_pixmap =
      CreateNativePixmapDmaBuf(input_frame.get());
  if (!input_pixmap) {
    VLOGF(1) << "Failed to create NativePixmap from VideoFrame";
    error_cb_.Run();
    return;
  }

  auto src_va_surface =
      vaapi_wrapper_->CreateVASurfaceForPixmap(std::move(input_pixmap));

  if (!src_va_surface)
    return;

  scoped_refptr<gfx::NativePixmap> output_pixmap =
      CreateNativePixmapDmaBuf(output_frame.get());
  if (!output_pixmap) {
    VLOGF(1) << "Failed to create NativePixmap from VideoFrame";
    error_cb_.Run();
    return;
  }

  auto dst_va_surface =
      vaapi_wrapper_->CreateVASurfaceForPixmap(std::move(output_pixmap));

  // Failed to create VASurface for frames. |cb| isn't executed in the case.
  if (!dst_va_surface)
    return;

  // VA-API performs pixel format conversion and scaling without any filters.
  if (!vaapi_wrapper_->BlitSurface(*src_va_surface, *dst_va_surface,
                                   input_frame->visible_rect(),
                                   output_frame->visible_rect())) {
    // Failed to execute BlitSurface(). Since VaapiWrapper has invoked
    // ReportToUMA(), calling error_cb_ here is not needed.
    return;
  }

  std::move(cb).Run(std::move(output_frame));
}

}  // namespace media
