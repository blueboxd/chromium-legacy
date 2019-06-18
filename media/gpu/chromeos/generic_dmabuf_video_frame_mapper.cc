// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/generic_dmabuf_video_frame_mapper.h"

#include <sys/mman.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "media/gpu/macros.h"

namespace media {

namespace {

uint8_t* Mmap(const size_t length, const int fd) {
  void* addr =
      mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0u);
  if (addr == MAP_FAILED) {
    VLOGF(1) << "Failed to mmap.";
    return nullptr;
  }
  return static_cast<uint8_t*>(addr);
}

void MunmapBuffers(const std::vector<std::pair<uint8_t*, size_t>>& chunks,
                   scoped_refptr<const VideoFrame> video_frame) {
  for (const auto& chunk : chunks) {
    DLOG_IF(ERROR, !chunk.first) << "Pointer to be released is nullptr.";
    munmap(chunk.first, chunk.second);
  }
}

// Create VideoFrame whose dtor unmaps memory in mapped planes referred
// by |plane_addrs|. |plane_addrs| are addresses to (Y, U, V) in this order.
// |chunks| is the vector of pair of (address, size) to be called in munmap().
// |src_video_frame| is the video frame that owns dmabufs to the mapped planes.
scoped_refptr<VideoFrame> CreateMappedVideoFrame(
    scoped_refptr<const VideoFrame> src_video_frame,
    uint8_t* plane_addrs[VideoFrame::kMaxPlanes],
    const std::vector<std::pair<uint8_t*, size_t>>& chunks) {
  scoped_refptr<VideoFrame> video_frame;

  const auto& layout = src_video_frame->layout();
  const auto& visible_rect = src_video_frame->visible_rect();
  if (IsYuvPlanar(layout.format())) {
    video_frame = VideoFrame::WrapExternalYuvDataWithLayout(
        layout, visible_rect, visible_rect.size(), plane_addrs[0],
        plane_addrs[1], plane_addrs[2], src_video_frame->timestamp());
  } else if (VideoFrame::NumPlanes(layout.format()) == 1) {
    size_t plane_size =
        VideoFrame::AllocationSize(layout.format(), layout.coded_size());
    if (plane_size > layout.buffer_sizes()[0] - layout.planes()[0].offset) {
      // VideoFrameLayout can describe planes that are larger than the useful
      // data they contain.
      VLOGF(1) << "buffer_size - offset is smaller than expected"
               << "buffer_size=" << layout.buffer_sizes()[0]
               << ", offset=" << layout.planes()[0].offset
               << ", plane_size=" << plane_size;
      return nullptr;
    }

    video_frame = VideoFrame::WrapExternalDataWithLayout(
        layout, visible_rect, visible_rect.size(), plane_addrs[0], plane_size,
        src_video_frame->timestamp());
  }
  if (!video_frame) {
    return nullptr;
  }

  // Pass org_video_frame so that it outlives video_frame.
  video_frame->AddDestructionObserver(
      base::BindOnce(MunmapBuffers, chunks, std::move(src_video_frame)));
  return video_frame;
}

bool IsFormatSupported(VideoPixelFormat format) {
  constexpr VideoPixelFormat supported_formats[] = {
      // RGB pixel formats.
      PIXEL_FORMAT_ABGR,
      PIXEL_FORMAT_ARGB,
      PIXEL_FORMAT_XBGR,

      // YUV pixel formats.
      PIXEL_FORMAT_I420,
      PIXEL_FORMAT_NV12,
      PIXEL_FORMAT_YV12,
  };
  return std::find(std::cbegin(supported_formats), std::cend(supported_formats),
                   format);
}

}  // namespace

// static
std::unique_ptr<GenericDmaBufVideoFrameMapper>
GenericDmaBufVideoFrameMapper::Create(VideoPixelFormat format) {
  if (!IsFormatSupported(format)) {
    VLOGF(1) << "Unsupported format: " << format;
    return nullptr;
  }
  return base::WrapUnique(new GenericDmaBufVideoFrameMapper(format));
}

GenericDmaBufVideoFrameMapper::GenericDmaBufVideoFrameMapper(
    VideoPixelFormat format)
    : VideoFrameMapper(format) {}

scoped_refptr<VideoFrame> GenericDmaBufVideoFrameMapper::Map(
    scoped_refptr<const VideoFrame> video_frame) const {
  if (video_frame->storage_type() != VideoFrame::StorageType::STORAGE_DMABUFS) {
    VLOGF(1) << "VideoFrame's storage type is not DMABUF: "
             << video_frame->storage_type();
    return nullptr;
  }

  if (video_frame->format() != format_) {
    VLOGF(1) << "Unexpected format: " << video_frame->format()
             << ", expected: " << format_;
    return nullptr;
  }

  const VideoFrameLayout& layout = video_frame->layout();

  // Map all buffers from their start address.
  const auto& dmabuf_fds = video_frame->DmabufFds();
  std::vector<std::pair<uint8_t*, size_t>> chunks;
  const auto& buffer_sizes = layout.buffer_sizes();
  std::vector<uint8_t*> buffer_addrs(buffer_sizes.size(), nullptr);
  DCHECK_LE(buffer_addrs.size(), dmabuf_fds.size());
  DCHECK_LE(buffer_addrs.size(), VideoFrame::kMaxPlanes);
  for (size_t i = 0; i < buffer_sizes.size(); i++) {
    buffer_addrs[i] = Mmap(buffer_sizes[i], dmabuf_fds[i].get());
    if (!buffer_addrs[i]) {
      MunmapBuffers(chunks, std::move(video_frame));
      return nullptr;
    }
    chunks.emplace_back(buffer_addrs[i], buffer_sizes[i]);
  }

  // Always prepare VideoFrame::kMaxPlanes addresses for planes initialized by
  // nullptr. This enables to specify nullptr to redundant plane, for pixel
  // format whose number of planes are less than VideoFrame::kMaxPlanes.
  const auto& planes = layout.planes();
  const size_t num_of_planes = layout.num_planes();
  uint8_t* plane_addrs[VideoFrame::kMaxPlanes] = {};
  for (size_t i = 0; i < num_of_planes; i++) {
    uint8_t* buffer =
        i < buffer_addrs.size() ? buffer_addrs[i] : buffer_addrs.back();
    plane_addrs[i] = buffer + planes[i].offset;
  }
  return CreateMappedVideoFrame(std::move(video_frame), plane_addrs, chunks);
}

}  // namespace media
