// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/fake_gpu_memory_buffer.h"

#include "build/build_config.h"

#if defined(OS_CHROMEOS)
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace media {

#if defined(OS_CHROMEOS)
base::ScopedFD GetDummyFD() {
  base::ScopedFD fd(open("/dev/zero", O_RDONLY));
  DCHECK(fd.is_valid());
  return fd;
}
#endif

FakeGpuMemoryBuffer::FakeGpuMemoryBuffer(const gfx::Size& size,
                                         gfx::BufferFormat format)
    : size_(size), format_(format) {
  // We use only NV12 or R8 in unit tests.
  CHECK(format == gfx::BufferFormat::YUV_420_BIPLANAR ||
        format == gfx::BufferFormat::R_8);

  size_t y_plane_size = size_.width() * size_.height();
  size_t uv_plane_size = size_.width() * size_.height() / 2;
  data_ = std::vector<uint8_t>(y_plane_size + uv_plane_size);

  handle_.type = gfx::NATIVE_PIXMAP;
  // Set a dummy id since this is for testing only.
  handle_.id = gfx::GpuMemoryBufferId(0);

#if defined(OS_CHROMEOS)
  // Set a dummy fd since this is for testing only.
  handle_.native_pixmap_handle.planes.push_back(
      gfx::NativePixmapPlane(size_.width(), 0, y_plane_size, GetDummyFD()));
  if (format == gfx::BufferFormat::YUV_420_BIPLANAR) {
    handle_.native_pixmap_handle.planes.push_back(gfx::NativePixmapPlane(
        size_.width(), handle_.native_pixmap_handle.planes[0].size,
        uv_plane_size, GetDummyFD()));
  }
#endif
}

FakeGpuMemoryBuffer::~FakeGpuMemoryBuffer() = default;

bool FakeGpuMemoryBuffer::Map() {
  return true;
}

void* FakeGpuMemoryBuffer::memory(size_t plane) {
  auto* data_ptr = data_.data();
  size_t y_plane_size = size_.width() * size_.height();
  switch (plane) {
    case 0:
      return reinterpret_cast<void*>(data_ptr);
    case 1:
      return reinterpret_cast<void*>(data_ptr + y_plane_size);
    default:
      NOTREACHED() << "Unsupported plane: " << plane;
      return nullptr;
  }
}

void FakeGpuMemoryBuffer::Unmap() {}

gfx::Size FakeGpuMemoryBuffer::GetSize() const {
  return size_;
}

gfx::BufferFormat FakeGpuMemoryBuffer::GetFormat() const {
  return format_;
}

int FakeGpuMemoryBuffer::stride(size_t plane) const {
  switch (plane) {
    case 0:
      return size_.width();
    case 1:
      return size_.width();
    default:
      NOTREACHED() << "Unsupported plane: " << plane;
      return 0;
  }
}

void FakeGpuMemoryBuffer::SetColorSpace(const gfx::ColorSpace& color_space) {}

gfx::GpuMemoryBufferId FakeGpuMemoryBuffer::GetId() const {
  return handle_.id;
}

gfx::GpuMemoryBufferType FakeGpuMemoryBuffer::GetType() const {
  return gfx::NATIVE_PIXMAP;
}

gfx::GpuMemoryBufferHandle FakeGpuMemoryBuffer::CloneHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::NATIVE_PIXMAP;
  handle.id = handle_.id;
#if defined(OS_LINUX)
  handle.native_pixmap_handle =
      gfx::CloneHandleForIPC(handle_.native_pixmap_handle);
#endif
  return handle;
}

ClientBuffer FakeGpuMemoryBuffer::AsClientBuffer() {
  NOTREACHED();
  return ClientBuffer();
}

void FakeGpuMemoryBuffer::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
    uint64_t tracing_process_id,
    int importance) const {}

}  // namespace media
