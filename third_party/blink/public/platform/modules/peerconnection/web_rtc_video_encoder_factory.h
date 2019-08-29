// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_WEB_RTC_VIDEO_ENCODER_FACTORY_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_WEB_RTC_VIDEO_ENCODER_FACTORY_H_

#include <memory>

#include "media/base/video_codecs.h"
#include "third_party/blink/public/platform/web_common.h"

namespace webrtc {
class VideoEncoder;
}

namespace media {
class GpuVideoAcceleratorFactories;
}

namespace blink {

// TODO(crbug.com/787254): Remove this API when its clients are Onion souped.
BLINK_PLATFORM_EXPORT std::unique_ptr<webrtc::VideoEncoder>
CreateRTCVideoEncoder(media::VideoCodecProfile profile,
                      media::GpuVideoAcceleratorFactories* gpu_factories);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_WEB_RTC_VIDEO_ENCODER_FACTORY_H_
