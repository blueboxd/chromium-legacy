// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/lite_video/lite_video_hint_agent.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/renderer/lite_video/lite_video_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace lite_video {

// Default bandwidth and latency parameters for throttling.
constexpr uint64_t kTargetDownlinkBandwidthKBPS = 400;
constexpr base::TimeDelta kTargetDownlinkRTTLatency =
    base::TimeDelta::FromMilliseconds(500);

// How much initial media bytes should be left unthrottled to alleviate pauses
// in the initial video play.
const uint64_t kkilobytesToBufferBeforeThrottle = 10;

// Maximum delay imposed for an response.
const base::TimeDelta kMaxResponseDelay = base::TimeDelta::FromSeconds(5);

LiteVideoHintAgent::LiteVideoHintAgent(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<LiteVideoHintAgent>(render_frame) {
  DCHECK(render_frame);
  DCHECK(IsLiteVideoEnabled());
  have_lite_video_hint_ = true;
  target_downlink_bandwidth_kbps_ = kTargetDownlinkBandwidthKBPS;
  target_downlink_rtt_latency_ = kTargetDownlinkRTTLatency;
  kilobytes_to_buffer_before_throttle_ = kkilobytesToBufferBeforeThrottle;
  UMA_HISTOGRAM_BOOLEAN("LiteVideo.HintAgent.HasHint", have_lite_video_hint_);
}

LiteVideoHintAgent::~LiteVideoHintAgent() = default;

void LiteVideoHintAgent::OnDestruct() {
  delete this;
}

base::TimeDelta LiteVideoHintAgent::CalculateLatencyForResourceResponse(
    const network::mojom::URLResponseHead& response_head) {
  if (!have_lite_video_hint_)
    return base::TimeDelta();

  int64_t recv_bytes = response_head.content_length;
  if (recv_bytes == -1)
    recv_bytes = response_head.encoded_body_length;
  if (recv_bytes == -1)
    return base::TimeDelta();

  if (kilobytes_buffered_before_throttle_ <
      kilobytes_to_buffer_before_throttle_) {
    kilobytes_buffered_before_throttle_ += recv_bytes / 1024;
    return base::TimeDelta();
  }

  // The total RTT for this media response should be based on how much time it
  // took to transfer the packet in the target bandwidth, and the per RTT
  // latency. For example, assuming 100KBPS target bandwidth and target RTT of 1
  // second, an 400KB response should have total delay of 5 seconds
  // (400/100 + 1).
  auto delay_for_throttled_response =
      base::TimeDelta::FromSecondsD(
          recv_bytes / (target_downlink_bandwidth_kbps_ * 1024.0)) +
      target_downlink_rtt_latency_;
  auto response_delay =
      response_head.response_time - response_head.request_time;
  if (delay_for_throttled_response <= response_delay)
    return base::TimeDelta();

  return std::min(delay_for_throttled_response - response_delay,
                  kMaxResponseDelay);
}

}  // namespace lite_video
