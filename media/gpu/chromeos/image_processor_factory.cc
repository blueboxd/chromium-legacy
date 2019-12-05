// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/image_processor_factory.h"

#include <stddef.h>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/chromeos/libyuv_image_processor.h"
#include "media/gpu/macros.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_image_processor.h"
#endif  // BUILDFLAG(USE_VAAPI)

#if BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_image_processor.h"
#include "media/gpu/v4l2/v4l2_vda_helpers.h"
#endif  // BUILDFLAG(USE_V4L2_CODEC)

namespace media {

namespace {

#if BUILDFLAG(USE_V4L2_CODEC)
std::unique_ptr<ImageProcessor> CreateV4L2ImageProcessorWithInputCandidates(
    const std::vector<std::pair<Fourcc, gfx::Size>>& input_candidates,
    const gfx::Size& visible_size,
    size_t num_buffers,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    ImageProcessorFactory::PickFormatCB out_format_picker,
    ImageProcessor::ErrorCB error_cb) {
  // Pick a renderable output format, and try each available input format.
  // TODO(akahuang): let |out_format_picker| return a list of supported output
  // formats, and try all combination of input/output format, if any platform
  // fails to create ImageProcessor via current approach.
  const std::vector<uint32_t> supported_output_formats =
      V4L2ImageProcessor::GetSupportedOutputFormats();
  std::vector<Fourcc> supported_fourccs;
  for (const auto& format : supported_output_formats)
    supported_fourccs.push_back(Fourcc::FromV4L2PixFmt(format));

  const uint32_t output_format =
      out_format_picker.Run(supported_fourccs).ToV4L2PixFmt();
  if (!output_format)
    return nullptr;

  const auto supported_input_pixfmts =
      V4L2ImageProcessor::GetSupportedInputFormats();
  for (const auto& input_candidate : input_candidates) {
    const uint32_t input_pixfmt = input_candidate.first.ToV4L2PixFmt();
    const gfx::Size& input_size = input_candidate.second;

    if (std::find(supported_input_pixfmts.begin(),
                  supported_input_pixfmts.end(),
                  input_pixfmt) == supported_input_pixfmts.end()) {
      continue;
    }

    // Try to get an image size as close as possible to the final size.
    gfx::Size output_size(visible_size.width(), visible_size.height());
    size_t num_planes = 0;
    if (!V4L2ImageProcessor::TryOutputFormat(input_pixfmt, output_format,
                                             input_size, &output_size,
                                             &num_planes)) {
      VLOGF(2) << "Failed to get output size and plane count of IP";
      continue;
    }

    return v4l2_vda_helpers::CreateImageProcessor(
        input_pixfmt, output_format, input_size, output_size, visible_size,
        num_buffers, V4L2Device::Create(), ImageProcessor::OutputMode::IMPORT,
        std::move(client_task_runner), std::move(error_cb));
  }
  return nullptr;
}
#endif  // BUILDFLAG(USE_V4L2_CODEC)

}  // namespace

// static
std::unique_ptr<ImageProcessor> ImageProcessorFactory::Create(
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    const std::vector<ImageProcessor::OutputMode>& preferred_output_modes,
    size_t num_buffers,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    ImageProcessor::ErrorCB error_cb) {
  std::unique_ptr<ImageProcessor> image_processor;
#if BUILDFLAG(USE_VAAPI)
  image_processor = VaapiImageProcessor::Create(input_config, output_config,
                                                preferred_output_modes,
                                                client_task_runner, error_cb);
  if (image_processor)
    return image_processor;
#endif  // BUILDFLAG(USE_VAAPI)
#if BUILDFLAG(USE_V4L2_CODEC)
  for (auto output_mode : preferred_output_modes) {
    image_processor = V4L2ImageProcessor::Create(
        client_task_runner, V4L2Device::Create(), input_config, output_config,
        output_mode, num_buffers, error_cb);
    if (image_processor)
      return image_processor;
  }
#endif  // BUILDFLAG(USE_V4L2_CODEC)
  for (auto output_mode : preferred_output_modes) {
    image_processor = LibYUVImageProcessor::Create(
        input_config, output_config, output_mode, client_task_runner, error_cb);
    if (image_processor)
      return image_processor;
  }
  return nullptr;
}

// static
std::unique_ptr<ImageProcessor>
ImageProcessorFactory::CreateWithInputCandidates(
    const std::vector<std::pair<Fourcc, gfx::Size>>& input_candidates,
    const gfx::Size& visible_size,
    size_t num_buffers,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    PickFormatCB out_format_picker,
    ImageProcessor::ErrorCB error_cb) {
#if BUILDFLAG(USE_V4L2_CODEC)
  auto processor = CreateV4L2ImageProcessorWithInputCandidates(
      input_candidates, visible_size, num_buffers, client_task_runner,
      out_format_picker, error_cb);
  if (processor)
    return processor;
#endif  // BUILDFLAG(USE_V4L2_CODEC)

  // TODO(crbug.com/1004727): Implement LibYUVImageProcessor and
  // VaapiImageProcessor.
  return nullptr;
}

}  // namespace media
