// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_IOS_CHROME_DEFAULT_BROWSER_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_METRICS_IOS_CHROME_DEFAULT_BROWSER_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

// IOSChromeStabilityMetricsProvider records iOS default-browser related
// metrics.
class IOSChromeDefaultBrowserMetricsProvider : public metrics::MetricsProvider {
 public:
  explicit IOSChromeDefaultBrowserMetricsProvider();
  ~IOSChromeDefaultBrowserMetricsProvider() override;

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(IOSChromeDefaultBrowserMetricsProvider);
};

#endif  // IOS_CHROME_BROWSER_METRICS_IOS_CHROME_DEFAULT_BROWSER_METRICS_PROVIDER_H_
