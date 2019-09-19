// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_VIDEO_CODEC_FACTORY_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_VIDEO_CODEC_FACTORY_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/webrtc/api/video_codecs/video_decoder_factory.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_factory.h"

namespace media {
class GpuVideoAcceleratorFactories;
}

namespace blink {

// TODO(crbug.com/787254): Remove these APIs when their clients are Onion
// souped.
BLINK_PLATFORM_EXPORT std::unique_ptr<webrtc::VideoEncoderFactory>
CreateWebrtcVideoEncoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories);
BLINK_PLATFORM_EXPORT std::unique_ptr<webrtc::VideoDecoderFactory>
CreateWebrtcVideoDecoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_VIDEO_CODEC_FACTORY_H_
