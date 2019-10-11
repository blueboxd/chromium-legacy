// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/peerconnection/webrtc_stats_report_obtainer.h"

#include "base/bind.h"
#include "base/callback.h"

namespace blink {

WebRTCStatsReportObtainer::WebRTCStatsReportObtainer() {}

WebRTCStatsReportObtainer::~WebRTCStatsReportObtainer() {}

blink::WebRTCStatsReportCallback
WebRTCStatsReportObtainer::GetStatsCallbackWrapper() {
  return base::BindOnce(&WebRTCStatsReportObtainer::OnStatsDelivered, this);
}

blink::WebRTCStatsReport* WebRTCStatsReportObtainer::report() const {
  return report_.get();
}

blink::WebRTCStatsReport* WebRTCStatsReportObtainer::WaitForReport() {
  run_loop_.Run();
  return report_.get();
}

void WebRTCStatsReportObtainer::OnStatsDelivered(
    std::unique_ptr<blink::WebRTCStatsReport> report) {
  report_ = std::move(report);
  run_loop_.Quit();
}

}  // namespace blink
