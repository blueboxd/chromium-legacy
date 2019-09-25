// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/shared_image_video_provider.h"

namespace media {

SharedImageVideoProvider::ImageSpec::ImageSpec(const gfx::Size& our_size)
    : size(our_size) {}
SharedImageVideoProvider::ImageSpec::ImageSpec(const ImageSpec&) = default;
SharedImageVideoProvider::ImageSpec::~ImageSpec() = default;

SharedImageVideoProvider::ImageRecord::ImageRecord() = default;
SharedImageVideoProvider::ImageRecord::ImageRecord(ImageRecord&&) = default;
SharedImageVideoProvider::ImageRecord::~ImageRecord() = default;

}  // namespace media
