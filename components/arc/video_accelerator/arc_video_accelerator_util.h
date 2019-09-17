// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_VIDEO_ACCELERATOR_ARC_VIDEO_ACCELERATOR_UTIL_H_
#define COMPONENTS_ARC_VIDEO_ACCELERATOR_ARC_VIDEO_ACCELERATOR_UTIL_H_

#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/optional.h"
#include "components/arc/video_accelerator/video_frame_plane.h"
#include "media/base/video_types.h"
#include "mojo/public/cpp/system/handle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace arc {

// Creates ScopedFD from given mojo::ScopedHandle.
// Returns invalid ScopedFD on failure.
base::ScopedFD UnwrapFdFromMojoHandle(mojo::ScopedHandle handle);

// Return the file size of |fd| in bytes.
bool GetFileSize(const int fd, size_t* size);

// Return GpuMemoryBufferHandle iff |planes| are valid for a video frame located
// on |fd| and of |pixel_format| and |coded_size|. Otherwise returns
// base::nullopt.
base::Optional<gfx::GpuMemoryBufferHandle> CreateGpuMemoryBufferHandle(
    media::VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    base::ScopedFD fd,
    const std::vector<VideoFramePlane>& planes);

// Create a temp file and write |data| into the file.
base::ScopedFD CreateTempFileForTesting(const std::string& data);

}  // namespace arc
#endif  // COMPONENTS_ARC_VIDEO_ACCELERATOR_ARC_VIDEO_ACCELERATOR_UTIL_H_
