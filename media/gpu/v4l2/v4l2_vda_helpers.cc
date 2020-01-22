// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_vda_helpers.h"

#include "base/bind.h"
#include "media/base/color_plane_layout.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_image_processor_backend.h"

namespace media {
namespace v4l2_vda_helpers {

base::Optional<Fourcc> FindImageProcessorInputFormat(V4L2Device* vda_device) {
  std::vector<uint32_t> processor_input_formats =
      V4L2ImageProcessorBackend::GetSupportedInputFormats();

  struct v4l2_fmtdesc fmtdesc = {};
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  while (vda_device->Ioctl(VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
    if (std::find(processor_input_formats.begin(),
                  processor_input_formats.end(),
                  fmtdesc.pixelformat) != processor_input_formats.end()) {
      DVLOGF(3) << "Image processor input format=" << fmtdesc.description;
      return Fourcc::FromV4L2PixFmt(fmtdesc.pixelformat);
    }
    ++fmtdesc.index;
  }
  return base::nullopt;
}

base::Optional<Fourcc> FindImageProcessorOutputFormat(V4L2Device* ip_device) {
  // Prefer YVU420 and NV12 because ArcGpuVideoDecodeAccelerator only supports
  // single physical plane.
  static constexpr uint32_t kPreferredFormats[] = {V4L2_PIX_FMT_NV12,
                                                   V4L2_PIX_FMT_YVU420};
  auto preferred_formats_first = [](uint32_t a, uint32_t b) -> bool {
    auto* iter_a = std::find(std::begin(kPreferredFormats),
                             std::end(kPreferredFormats), a);
    auto* iter_b = std::find(std::begin(kPreferredFormats),
                             std::end(kPreferredFormats), b);
    return iter_a < iter_b;
  };

  std::vector<uint32_t> processor_output_formats =
      V4L2ImageProcessorBackend::GetSupportedOutputFormats();

  // Move the preferred formats to the front.
  std::sort(processor_output_formats.begin(), processor_output_formats.end(),
            preferred_formats_first);

  for (uint32_t processor_output_format : processor_output_formats) {
    auto fourcc = Fourcc::FromV4L2PixFmt(processor_output_format);
    if (fourcc && ip_device->CanCreateEGLImageFrom(*fourcc)) {
      DVLOGF(3) << "Image processor output format=" << processor_output_format;
      return fourcc;
    }
  }

  return base::nullopt;
}

std::unique_ptr<ImageProcessor> CreateImageProcessor(
    const Fourcc vda_output_format,
    const Fourcc ip_output_format,
    const gfx::Size& vda_output_coded_size,
    const gfx::Size& ip_output_coded_size,
    const gfx::Size& visible_size,
    size_t nb_buffers,
    scoped_refptr<V4L2Device> image_processor_device,
    ImageProcessor::OutputMode image_processor_output_mode,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    ImageProcessor::ErrorCB error_cb) {
  // TODO(crbug.com/917798): Use ImageProcessorFactory::Create() once we remove
  //     |image_processor_device_| from V4L2VideoDecodeAccelerator.
  auto image_processor = ImageProcessor::Create(
      base::BindRepeating(&V4L2ImageProcessorBackend::Create,
                          image_processor_device, nb_buffers),
      ImageProcessor::PortConfig(vda_output_format, vda_output_coded_size, {},
                                 gfx::Rect(visible_size),
                                 {VideoFrame::STORAGE_DMABUFS}),
      ImageProcessor::PortConfig(ip_output_format, ip_output_coded_size, {},
                                 gfx::Rect(visible_size),
                                 {VideoFrame::STORAGE_DMABUFS}),
      {image_processor_output_mode}, std::move(error_cb),
      std::move(client_task_runner));
  if (!image_processor)
    return nullptr;

  if (image_processor->output_config().size != ip_output_coded_size) {
    VLOGF(1) << "Image processor should be able to use the requested output "
             << "coded size " << ip_output_coded_size.ToString()
             << " without adjusting to "
             << image_processor->output_config().size.ToString();
    return nullptr;
  }

  if (image_processor->input_config().size != vda_output_coded_size) {
    VLOGF(1) << "Image processor should be able to take the output coded "
             << "size of decoder " << vda_output_coded_size.ToString()
             << " without adjusting to "
             << image_processor->input_config().size.ToString();
    return nullptr;
  }

  return image_processor;
}

gfx::Size NativePixmapSizeFromHandle(const gfx::NativePixmapHandle& handle,
                                     const Fourcc fourcc,
                                     const gfx::Size& current_size) {
  const uint32_t stride = handle.planes[0].stride;
  const uint32_t horiz_bits_per_pixel =
      VideoFrame::PlaneHorizontalBitsPerPixel(fourcc.ToVideoPixelFormat(), 0);
  DCHECK_NE(horiz_bits_per_pixel, 0u);
  // Stride must fit exactly on a byte boundary (8 bits per byte)
  DCHECK_EQ((stride * 8) % horiz_bits_per_pixel, 0u);

  // Actual width of buffer is stride (in bits) divided by bits per pixel.
  int adjusted_coded_width = stride * 8 / horiz_bits_per_pixel;
  // If the buffer is multi-planar, then the height of the buffer does not
  // matter as long as it covers the visible area and we can just return
  // the current height.
  // For single-planar however, the actual height can be inferred by dividing
  // the start offset of the second plane by the stride of the first plane,
  // since the second plane is supposed to start right after the first one.
  int adjusted_coded_height =
      handle.planes.size() > 1 && handle.planes[1].offset != 0
          ? handle.planes[1].offset / adjusted_coded_width
          : current_size.height();

  DCHECK_GE(adjusted_coded_width, current_size.width());
  DCHECK_GE(adjusted_coded_height, current_size.height());

  return gfx::Size(adjusted_coded_width, adjusted_coded_height);
}

}  // namespace v4l2_vda_helpers
}  // namespace media
