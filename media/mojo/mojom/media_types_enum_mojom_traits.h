// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_MEDIA_TYPES_ENUM_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_MEDIA_TYPES_ENUM_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_transformation.h"
#include "media/mojo/mojom/media_types.mojom-shared.h"

// Most enums have automatically generated traits, in media_types.mojom.h, due
// to their [native] attribute. This file defines traits for enums that are used
// in files that cannot directly include media_types.mojom.h.

namespace mojo {

template <>
struct EnumTraits<media::mojom::VideoRotation, ::media::VideoRotation> {
  static media::mojom::VideoRotation ToMojom(::media::VideoRotation input) {
    switch (input) {
      case ::media::VideoRotation::VIDEO_ROTATION_0:
        return media::mojom::VideoRotation::kVideoRotation0;
      case ::media::VideoRotation::VIDEO_ROTATION_90:
        return media::mojom::VideoRotation::kVideoRotation90;
      case ::media::VideoRotation::VIDEO_ROTATION_180:
        return media::mojom::VideoRotation::kVideoRotation180;
      case ::media::VideoRotation::VIDEO_ROTATION_270:
        return media::mojom::VideoRotation::kVideoRotation270;
    }

    NOTREACHED();
    return static_cast<media::mojom::VideoRotation>(input);
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(media::mojom::VideoRotation input,
                        media::VideoRotation* output) {
    switch (input) {
      case media::mojom::VideoRotation::kVideoRotation0:
        *output = ::media::VideoRotation::VIDEO_ROTATION_0;
        return true;
      case media::mojom::VideoRotation::kVideoRotation90:
        *output = ::media::VideoRotation::VIDEO_ROTATION_90;
        return true;
      case media::mojom::VideoRotation::kVideoRotation180:
        *output = ::media::VideoRotation::VIDEO_ROTATION_180;
        return true;
      case media::mojom::VideoRotation::kVideoRotation270:
        *output = ::media::VideoRotation::VIDEO_ROTATION_270;
        return true;
    }

    NOTREACHED();
    *output = static_cast<::media::VideoRotation>(input);
    return true;
  }
};

template <>
struct EnumTraits<media::mojom::CopyMode,
                  ::media::VideoFrameMetadata::CopyMode> {
  static media::mojom::CopyMode ToMojom(
      ::media::VideoFrameMetadata::CopyMode input) {
    switch (input) {
      case ::media::VideoFrameMetadata::CopyMode::kCopyToNewTexture:
        return media::mojom::CopyMode::kCopyToNewTexture;
      case ::media::VideoFrameMetadata::CopyMode::kCopyMailboxesOnly:
        return media::mojom::CopyMode::kCopyMailboxesOnly;
    }

    NOTREACHED();
    return static_cast<media::mojom::CopyMode>(input);
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(media::mojom::CopyMode input,
                        media::VideoFrameMetadata::CopyMode* output) {
    switch (input) {
      case media::mojom::CopyMode::kCopyToNewTexture:
        *output = ::media::VideoFrameMetadata::CopyMode::kCopyToNewTexture;
        return true;
      case media::mojom::CopyMode::kCopyMailboxesOnly:
        *output = ::media::VideoFrameMetadata::CopyMode::kCopyMailboxesOnly;
        return true;
    }

    NOTREACHED();
    *output = static_cast<::media::VideoFrameMetadata::CopyMode>(input);
    return false;
  }
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_MEDIA_TYPES_ENUM_MOJOM_TRAITS_H_
