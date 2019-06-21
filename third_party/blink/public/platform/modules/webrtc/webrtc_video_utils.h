// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_WEBRTC_WEBRTC_VIDEO_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_WEBRTC_WEBRTC_VIDEO_UTILS_H_

#include "media/base/video_color_space.h"
#include "media/base/video_transformation.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/webrtc/api/video/color_space.h"
#include "third_party/webrtc/api/video/video_rotation.h"

namespace blink {

// This file has helper methods for conversion between chromium types and
// webrtc/api/video types.
//
// TODO(crbug.com/704136): Move the helper methods below out of the Blink
// exposed API when all users of it have been Onion souped.

media::VideoRotation BLINK_PLATFORM_EXPORT
WebRtcToMediaVideoRotation(webrtc::VideoRotation rotation);

media::VideoColorSpace BLINK_PLATFORM_EXPORT
WebRtcToMediaVideoColorSpace(const webrtc::ColorSpace& color_space);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_WEBRTC_WEBRTC_VIDEO_UTILS_H_
