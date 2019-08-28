// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_WEB_RTC_VIDEO_FRAME_ADAPTER_FACTORY_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_WEB_RTC_VIDEO_FRAME_ADAPTER_FACTORY_H_

#include <memory>

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"

namespace media {
class GpuVideoAcceleratorFactories;
}

namespace webrtc {
class VideoDecoder;
}

namespace blink {

// Creates and initializes an RTCVideoDecoderAdapter. Returns nullptr if
// |format| cannot be supported.
// Called on the worker thread.
//
// TODO(crbug.com/787254): Remove this API when its clients are Onion souped.
BLINK_PLATFORM_EXPORT std::unique_ptr<webrtc::VideoDecoder>
CreateRTCVideoDecoderAdapter(media::GpuVideoAcceleratorFactories* gpu_factories,
                             const webrtc::SdpVideoFormat& format);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_WEB_RTC_VIDEO_FRAME_ADAPTER_FACTORY_H_
