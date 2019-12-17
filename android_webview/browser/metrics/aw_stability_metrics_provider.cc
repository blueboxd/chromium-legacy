// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_stability_metrics_provider.h"

#include "base/macros.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace android_webview {

AwStabilityMetricsProvider::AwStabilityMetricsProvider(PrefService* local_state)
    : helper_(local_state) {
  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_HOST_HANG,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllSources());
}

AwStabilityMetricsProvider::~AwStabilityMetricsProvider() {
  registrar_.RemoveAll();
}

void AwStabilityMetricsProvider::OnRecordingEnabled() {}

void AwStabilityMetricsProvider::OnRecordingDisabled() {}

void AwStabilityMetricsProvider::ProvideStabilityMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  helper_.ProvideStabilityMetrics(system_profile_proto);
}

void AwStabilityMetricsProvider::ClearSavedStabilityMetrics() {
  helper_.ClearSavedStabilityMetrics();
}

void AwStabilityMetricsProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_START:
      helper_.LogLoadStarted();
      break;

    case content::NOTIFICATION_RENDER_WIDGET_HOST_HANG:
      helper_.LogRendererHang();
      break;

    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      helper_.LogRendererLaunched(/*was_extension_process=*/false);
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

}  // namespace android_webview
