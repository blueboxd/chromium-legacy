// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_RTC_VIDEO_DECODER_FACTORY_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_RTC_VIDEO_DECODER_FACTORY_H_

#include "base/macros.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/webrtc/api/video_codecs/video_decoder_factory.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"

namespace webrtc {
class VideoDecoder;
}  // namespace webrtc

namespace media {
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace blink {

// TODO(wuchengli): add unittest.
//
// TODO(crbug.com/787254): Move this class out of the Blink exposed API when its
// clients get Onion soup'ed. Also, switch away from using std::vector.
class BLINK_PLATFORM_EXPORT RTCVideoDecoderFactory
    : public webrtc::VideoDecoderFactory {
 public:
  explicit RTCVideoDecoderFactory(
      media::GpuVideoAcceleratorFactories* gpu_factories);
  ~RTCVideoDecoderFactory() override;

  // Runs on Chrome_libJingle_WorkerThread. The child thread is blocked while
  // this runs.
  std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(
      const webrtc::SdpVideoFormat& format) override;

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;

 private:
  media::GpuVideoAcceleratorFactories* gpu_factories_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoDecoderFactory);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_PEERCONNECTION_RTC_VIDEO_DECODER_FACTORY_H_
