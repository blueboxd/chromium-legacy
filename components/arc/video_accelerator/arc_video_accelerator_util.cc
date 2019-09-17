// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/video_accelerator/arc_video_accelerator_util.h"

#include <sys/types.h>
#include <unistd.h>

#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/video_frame.h"
#include "media/gpu/macros.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

namespace {

bool VerifyGpuMemoryBufferHandle(media::VideoPixelFormat pixel_format,
                                 const gfx::Size& coded_size,
                                 const gfx::GpuMemoryBufferHandle& gmb_handle) {
  if (gmb_handle.type != gfx::NATIVE_PIXMAP) {
    VLOGF(1) << "Unexpected GpuMemoryBufferType: " << gmb_handle.type;
    return false;
  }

  const size_t num_planes = media::VideoFrame::NumPlanes(pixel_format);
  if (num_planes != gmb_handle.native_pixmap_handle.planes.size() ||
      num_planes == 0) {
    VLOGF(1) << "Invalid number of dmabuf planes passed: "
             << gmb_handle.native_pixmap_handle.planes.size()
             << ", expected: " << num_planes;
    return false;
  }

  // Offsets monotonically increase and strides monotonically decrease.
  // Note: this offset assumption might not be correct if planes are stored in
  // multiple buffers.
  // TODO(b/127230761): Remove this offset check once one fd is given per one
  // plane.
  for (size_t i = 1; i < num_planes; i++) {
    if (gmb_handle.native_pixmap_handle.planes[i].offset <
        gmb_handle.native_pixmap_handle.planes[i - 1].offset) {
      return false;
    }
    if (gmb_handle.native_pixmap_handle.planes[i - 1].stride <
        gmb_handle.native_pixmap_handle.planes[i].stride) {
      return false;
    }
  }

  size_t prev_buffer_end = 0;
  for (size_t i = 0; i < num_planes; i++) {
    const auto& plane = gmb_handle.native_pixmap_handle.planes[i];
    DVLOGF(4) << "Plane " << i << ", offset: " << plane.offset
              << ", stride: " << plane.stride;

    size_t file_size_in_bytes;
    if (!plane.fd.is_valid() ||
        !GetFileSize(plane.fd.get(), &file_size_in_bytes))
      return false;

    size_t plane_height =
        media::VideoFrame::Rows(i, pixel_format, coded_size.height());
    base::CheckedNumeric<size_t> min_plane_size =
        base::CheckMul(plane.stride, plane_height);
    if (!min_plane_size.IsValid() || min_plane_size.ValueOrDie() > plane.size) {
      VLOGF(1) << "Invalid strides/sizes";
      return false;
    }

    // Check |offset| + (the size of a plane) on each plane is not larger than
    // |file_size_in_bytes|. This ensures we don't access out of a buffer
    // referred by |fd|.
    base::CheckedNumeric<size_t> min_buffer_size =
        base::CheckAdd(plane.offset, plane.size);
    if (!min_buffer_size.IsValid() ||
        min_buffer_size.ValueOrDie() > file_size_in_bytes) {
      VLOGF(1) << "Invalid strides/offsets";
      return false;
    }

    // The end of the previous plane must not be bigger than the offset of the
    // current plane.
    // TODO(b/127230761): Remove this check once one fd is given per one plane.
    if (prev_buffer_end > base::checked_cast<size_t>(plane.offset)) {
      VLOGF(1) << "Invalid offset";
      return false;
    }
    prev_buffer_end = min_buffer_size.ValueOrDie();
  }

  return true;
}

}  // namespace

base::ScopedFD UnwrapFdFromMojoHandle(mojo::ScopedHandle handle) {
  if (!handle.is_valid()) {
    VLOGF(1) << "Handle is invalid";
    return base::ScopedFD();
  }

  base::PlatformFile platform_file;
  MojoResult mojo_result =
      mojo::UnwrapPlatformFile(std::move(handle), &platform_file);
  if (mojo_result != MOJO_RESULT_OK) {
    VLOGF(1) << "UnwrapPlatformFile failed: " << mojo_result;
    return base::ScopedFD();
  }

  return base::ScopedFD(platform_file);
}

bool GetFileSize(const int fd, size_t* size) {
  if (fd < 0) {
    VLOGF(1) << "Invalid file descriptor";
    return false;
  }

  off_t fd_size = lseek(fd, 0, SEEK_END);
  lseek(fd, 0, SEEK_SET);
  if (fd_size < 0u) {
    VPLOGF(1) << "Fail to find the size of fd";
    return false;
  }

  if (!base::IsValueInRangeForNumericType<size_t>(fd_size)) {
    VLOGF(1) << "fd_size is out of range of size_t"
             << ", size=" << size
             << ", size_t max=" << std::numeric_limits<size_t>::max()
             << ", size_t min=" << std::numeric_limits<size_t>::min();
    return false;
  }

  *size = static_cast<size_t>(fd_size);
  return true;
}

base::Optional<gfx::GpuMemoryBufferHandle> CreateGpuMemoryBufferHandle(
    media::VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    base::ScopedFD fd,
    const std::vector<VideoFramePlane>& planes) {
  const size_t num_planes = media::VideoFrame::NumPlanes(pixel_format);
  if (planes.size() != num_planes || planes.size() == 0) {
    VLOGF(1) << "Invalid number of dmabuf planes passed: " << planes.size()
             << ", expected: " << num_planes;
    return base::nullopt;
  }

  std::array<base::ScopedFD, media::VideoFrame::kMaxPlanes> scoped_fds;
  DCHECK_LE(num_planes, media::VideoFrame::kMaxPlanes);
  scoped_fds[0] = std::move(fd);
  for (size_t i = 1; i < num_planes; ++i) {
    scoped_fds[i].reset(HANDLE_EINTR(dup(scoped_fds[0].get())));
    if (!scoped_fds[i].is_valid()) {
      VLOGF(1) << "Failed to duplicate fd";
      return base::nullopt;
    }
  }

  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::NATIVE_PIXMAP;
  for (size_t i = 0; i < num_planes; ++i) {
    // NOTE: planes[i].stride and planes[i].offset both are int32_t. stride and
    // offset in NativePixmapPlane are uint32_t and uint64_t, respectively.
    if (!base::IsValueInRangeForNumericType<uint32_t>(planes[i].stride)) {
      VLOGF(1) << "Invalid stride";
      return base::nullopt;
    }
    if (!base::IsValueInRangeForNumericType<uint64_t>(planes[i].offset)) {
      VLOGF(1) << "Invalid offset";
      return base::nullopt;
    }
    uint32_t stride = base::checked_cast<uint32_t>(planes[i].stride);
    uint64_t offset = base::checked_cast<uint64_t>(planes[i].offset);

    size_t plane_height =
        media::VideoFrame::Rows(i, pixel_format, coded_size.height());
    base::CheckedNumeric<uint64_t> current_size =
        base::CheckMul(stride, plane_height);
    if (!current_size.IsValid()) {
      VLOGF(1) << "Invalid stride/height";
      return base::nullopt;
    }
    gmb_handle.native_pixmap_handle.planes.emplace_back(
        stride, offset, current_size.ValueOrDie(), std::move(scoped_fds[i]));
  }

  if (!VerifyGpuMemoryBufferHandle(pixel_format, coded_size, gmb_handle))
    return base::nullopt;
  return gmb_handle;
}

base::ScopedFD CreateTempFileForTesting(const std::string& data) {
  base::FilePath path;
  base::CreateTemporaryFile(&path);
  if (base::WriteFile(path, data.c_str(), data.size()) !=
      base::MakeStrictNum(data.size())) {
    VLOGF(1) << "Cannot write the whole data into file.";
    return base::ScopedFD();
  }

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    VLOGF(1) << "Failed to create file.";
    return base::ScopedFD();
  }

  return base::ScopedFD(file.TakePlatformFile());
}

}  // namespace arc
