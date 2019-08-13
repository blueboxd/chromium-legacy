// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_SOURCE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/platform/web_rtc_rtp_source.h"
#include "third_party/webrtc/api/rtp_receiver_interface.h"

namespace content {

class CONTENT_EXPORT RTCRtpSource : public blink::WebRTCRtpSource {
 public:
  explicit RTCRtpSource(const webrtc::RtpSource& source);
  ~RTCRtpSource() override;

  blink::WebRTCRtpSource::Type SourceType() const override;
  base::TimeTicks Timestamp() const override;
  uint32_t Source() const override;
  base::Optional<double> AudioLevel() const override;
  uint32_t RtpTimestamp() const override;

 private:
  const webrtc::RtpSource source_;

  DISALLOW_COPY_AND_ASSIGN(RTCRtpSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_SOURCE_H_
