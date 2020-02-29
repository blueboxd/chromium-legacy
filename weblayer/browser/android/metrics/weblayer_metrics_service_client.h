// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_ANDROID_METRICS_WEBLAYER_METRICS_SERVICE_CLIENT_H_
#define WEBLAYER_BROWSER_ANDROID_METRICS_WEBLAYER_METRICS_SERVICE_CLIENT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/embedder_support/android/metrics/android_metrics_service_client.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace weblayer {

class WebLayerMetricsServiceClient
    : public ::metrics::AndroidMetricsServiceClient {
  friend class base::NoDestructor<WebLayerMetricsServiceClient>;

 public:
  static WebLayerMetricsServiceClient* GetInstance();

  WebLayerMetricsServiceClient();
  ~WebLayerMetricsServiceClient() override;

  // metrics::MetricsServiceClient
  int32_t GetProduct() override;

  // metrics::AndroidMetricsServiceClient:
  int GetSampleRatePerMille() override;
  void InitInternal() override;
  void OnMetricsStart() override;
  int GetPackageNameLimitRatePerMille() override;
  bool ShouldWakeMetricsService() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebLayerMetricsServiceClient);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_ANDROID_METRICS_WEBLAYER_METRICS_SERVICE_CLIENT_H_
