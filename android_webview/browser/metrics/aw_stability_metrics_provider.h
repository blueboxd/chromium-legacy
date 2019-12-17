// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_AW_STABILITY_METRICS_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_AW_STABILITY_METRICS_PROVIDER_H_

#include "base/macros.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/stability_metrics_helper.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefService;

namespace content {
class NotificationDetails;
class NotificationSource;
}  // namespace content

namespace metrics {
class SystemProfileProto;
}  // namespace metrics

namespace android_webview {

// AwStabilityMetricsProvider gathers and logs Content-embedder-specific
// stability-related metrics. TODO(ntfschr): once we've fully implemented
// stability metrics in AW, we should componentize most of this to be shared by
// weblayer and chrome. See https://crbug.com/1015655.
class AwStabilityMetricsProvider : public metrics::MetricsProvider,
                                   public content::NotificationObserver {
 public:
  explicit AwStabilityMetricsProvider(PrefService* local_state);
  ~AwStabilityMetricsProvider() override;

  // metrics::MetricsDataProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideStabilityMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;
  void ClearSavedStabilityMetrics() override;

 protected:
  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  metrics::StabilityMetricsHelper helper_;

  // Registrar for receiving stability-related notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AwStabilityMetricsProvider);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_AW_STABILITY_METRICS_PROVIDER_H_
