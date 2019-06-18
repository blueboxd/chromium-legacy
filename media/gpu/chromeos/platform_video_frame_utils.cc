// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/platform_video_frame_utils.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_file.h"
#include "build/build_config.h"
#include "media/base/scopedfd_helper.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/format_utils.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if defined(USE_OZONE)
#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

namespace media {

namespace {

#if defined(USE_OZONE)
scoped_refptr<VideoFrame> CreateVideoFrameOzone(VideoPixelFormat pixel_format,
                                                const gfx::Size& coded_size,
                                                const gfx::Rect& visible_rect,
                                                const gfx::Size& natural_size,
                                                base::TimeDelta timestamp,
                                                gfx::BufferUsage buffer_usage) {
  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  DCHECK(platform);
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();
  DCHECK(factory);

  auto buffer_format = VideoPixelFormatToGfxBufferFormat(pixel_format);
  if (!buffer_format)
    return nullptr;

  auto pixmap =
      factory->CreateNativePixmap(gfx::kNullAcceleratedWidget, VK_NULL_HANDLE,
                                  coded_size, *buffer_format, buffer_usage);
  if (!pixmap)
    return nullptr;

  const size_t num_planes = VideoFrame::NumPlanes(pixel_format);
  std::vector<VideoFrameLayout::Plane> planes(num_planes);
  std::vector<size_t> buffer_sizes(num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    planes[i].stride = pixmap->GetDmaBufPitch(i);
    planes[i].offset = pixmap->GetDmaBufOffset(i);
    buffer_sizes[i] = planes[i].offset +
                      planes[i].stride * VideoFrame::Rows(i, pixel_format,
                                                          coded_size.height());
  }
  auto layout = VideoFrameLayout::CreateWithPlanes(
      pixel_format, coded_size, std::move(planes), std::move(buffer_sizes),
      VideoFrameLayout::kBufferAddressAlignment,
      pixmap->GetBufferFormatModifier());

  if (!layout)
    return nullptr;

  std::vector<base::ScopedFD> dmabuf_fds;
  for (size_t i = 0; i < num_planes; ++i) {
    int duped_fd = HANDLE_EINTR(dup(pixmap->GetDmaBufFd(i)));
    if (duped_fd == -1) {
      DLOG(ERROR) << "Failed duplicating dmabuf fd";
      return nullptr;
    }
    dmabuf_fds.emplace_back(duped_fd);
  }

  auto frame = VideoFrame::WrapExternalDmabufs(
      *layout, visible_rect, visible_rect.size(), std::move(dmabuf_fds),
      timestamp);
  if (!frame)
    return nullptr;

  // created |pixmap| must be owned by |frame|.
  frame->AddDestructionObserver(
      base::BindOnce(base::DoNothing::Once<scoped_refptr<gfx::NativePixmap>>(),
                     std::move(pixmap)));
  return frame;
}
#endif  // defined(USE_OZONE)

}  // namespace

scoped_refptr<VideoFrame> CreatePlatformVideoFrame(
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferUsage buffer_usage) {
#if defined(USE_OZONE)
  return CreateVideoFrameOzone(pixel_format, coded_size, visible_rect,
                               natural_size, timestamp, buffer_usage);
#endif  // defined(USE_OZONE)
  NOTREACHED();
  return nullptr;
}

gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferHandle(
    const VideoFrame* video_frame) {
  DCHECK(video_frame);

  gfx::GpuMemoryBufferHandle handle;
#if defined(OS_LINUX)
  handle.type = gfx::NATIVE_PIXMAP;

  std::vector<base::ScopedFD> duped_fds =
      DuplicateFDs(video_frame->DmabufFds());
  const size_t num_planes = VideoFrame::NumPlanes(video_frame->format());
  const size_t num_buffers = video_frame->layout().buffer_sizes().size();
  DCHECK_EQ(video_frame->layout().planes().size(), num_planes);
  handle.native_pixmap_handle.modifier = video_frame->layout().modifier();
  for (size_t i = 0; i < num_planes; ++i) {
    const auto& plane = video_frame->layout().planes()[i];
    size_t buffer_size = 0;
    if (i < num_buffers)
      buffer_size = video_frame->layout().buffer_sizes()[i];
    handle.native_pixmap_handle.planes.emplace_back(
        plane.stride, plane.offset, buffer_size, std::move(duped_fds[i]));
  }
#else
  NOTREACHED();
#endif  // defined(OS_LINUX)
  return handle;
}

}  // namespace media
