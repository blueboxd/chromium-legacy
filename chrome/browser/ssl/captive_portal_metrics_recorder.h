// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CAPTIVE_PORTAL_METRICS_RECORDER_H_
#define CHROME_BROWSER_SSL_CAPTIVE_PORTAL_METRICS_RECORDER_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "net/cert/x509_certificate.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// This class helps the SSL interstitial record captive portal-specific
// metrics. It should only be used on the UI thread because its implementation
// uses captive_portal::CaptivePortalService which can only be accessed on the
// UI thread.
class CaptivePortalMetricsRecorder {
 public:
  CaptivePortalMetricsRecorder(content::WebContents* web_contents,
                               bool overridable);
  ~CaptivePortalMetricsRecorder();

  // Should be called when the interstitial is closing.
  void RecordCaptivePortalUMAStatistics() const;

 private:
  typedef std::vector<std::string> Tokens;

  void Observe(const CaptivePortalService::Results& results);

  bool overridable_;
  bool captive_portal_detection_enabled_;
  // Did the probe complete before the interstitial was closed?
  bool captive_portal_probe_completed_;
  // Did the captive portal probe receive an error or get a non-HTTP response?
  bool captive_portal_no_response_;
  bool captive_portal_detected_;

  std::unique_ptr<CaptivePortalService::Subscription> subscription_;
};

#endif  // CHROME_BROWSER_SSL_CAPTIVE_PORTAL_METRICS_RECORDER_H_
