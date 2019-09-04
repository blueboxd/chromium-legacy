// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_RTC_VIDEO_ENCODER_FACTORY_UTIL_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_RTC_VIDEO_ENCODER_FACTORY_UTIL_H_

#include <memory>

#include "third_party/blink/public/platform/web_common.h"

namespace media {
class GpuVideoAcceleratorFactories;
}

namespace webrtc {
class VideoEncoderFactory;
}

namespace blink {

// TODO(crbug.com/787254): Remove this API when its clients are Onion souped.
BLINK_PLATFORM_EXPORT std::unique_ptr<webrtc::VideoEncoderFactory>
CreateRTCVideoEncoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_RTC_VIDEO_ENCODER_FACTORY_UTIL_H_
