// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_metrics_service_client_delegate.h"

#include "android_webview/browser/lifecycle/aw_contents_lifecycle_notifier.h"
#include "android_webview/browser/page_load_metrics/aw_page_load_metrics_provider.h"
#include "components/metrics/metrics_service.h"

namespace android_webview {

AwMetricsServiceClientDelegate::AwMetricsServiceClientDelegate() = default;
AwMetricsServiceClientDelegate::~AwMetricsServiceClientDelegate() = default;

void AwMetricsServiceClientDelegate::RegisterAdditionalMetricsProviders(
    metrics::MetricsService* service) {
  service->RegisterMetricsProvider(
      std::make_unique<android_webview::AwPageLoadMetricsProvider>());
}

void AwMetricsServiceClientDelegate::AddWebViewAppStateObserver(
    WebViewAppStateObserver* observer) {
  AwContentsLifecycleNotifier::GetInstance().AddObserver(observer);
}

bool AwMetricsServiceClientDelegate::HasAwContentsEverCreated() const {
  return AwContentsLifecycleNotifier::GetInstance()
      .has_aw_contents_ever_created();
}

}  // namespace android_webview
