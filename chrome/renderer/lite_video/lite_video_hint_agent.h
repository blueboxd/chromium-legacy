// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_LITE_VIDEO_LITE_VIDEO_HINT_AGENT_H_
#define CHROME_RENDERER_LITE_VIDEO_LITE_VIDEO_HINT_AGENT_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/renderer/lite_video/lite_video_url_loader_throttle.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "url/gurl.h"

namespace lite_video {

// The renderer-side agent for LiteVideos. There is one instance per frame (main
// frame and subframes), to receive LiteVideo throttling parameters from
// browser.
class LiteVideoHintAgent
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<LiteVideoHintAgent> {
 public:
  explicit LiteVideoHintAgent(content::RenderFrame* render_frame);
  ~LiteVideoHintAgent() override;

  LiteVideoHintAgent(const LiteVideoHintAgent&) = delete;
  LiteVideoHintAgent& operator=(const LiteVideoHintAgent&) = delete;

  // Returns how much time the media response should get throttled. This is the
  // difference between the target latency based on target bandwidth, RTT, and
  // the latency the response has already spent. Empty duration is returned when
  // the response should not be throttled. The first
  // |kilobytes_buffered_before_throttle_| for this render frame should not be
  // throttled. This function also updates
  // |kilobytes_buffered_before_throttle_|.
  base::TimeDelta CalculateLatencyForResourceResponse(
      const network::mojom::URLResponseHead& response_head);

  bool have_lite_video_hint() const { return have_lite_video_hint_; }

 private:
  // content::RenderFrameObserver overrides
  void OnDestruct() override;

  bool have_lite_video_hint_ = false;

  int target_downlink_bandwidth_kbps_;
  base::TimeDelta target_downlink_rtt_latency_;
  int kilobytes_to_buffer_before_throttle_;

  // How many initial media KB should be left unthrottled to alleviate pauses
  // in the initial video play.
  int kilobytes_buffered_before_throttle_ = 0;
};

}  // namespace lite_video

#endif  // CHROME_RENDERER_LITE_VIDEO_LITE_VIDEO_HINT_AGENT_H_
