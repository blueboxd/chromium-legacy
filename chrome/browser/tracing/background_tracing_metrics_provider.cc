// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/background_tracing_metrics_provider.h"

#include <utility>

#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "components/metrics/field_trials_provider.h"
#include "content/public/browser/background_tracing_manager.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/trace_log.pb.h"

namespace tracing {

BackgroundTracingMetricsProvider::BackgroundTracingMetricsProvider() {}
BackgroundTracingMetricsProvider::~BackgroundTracingMetricsProvider() {}

void BackgroundTracingMetricsProvider::Init() {
  // TODO(ssid): SetupBackgroundTracingFieldTrial() should be called here.
}

bool BackgroundTracingMetricsProvider::HasIndependentMetrics() {
  return content::BackgroundTracingManager::GetInstance()->HasTraceToUpload();
}

void BackgroundTracingMetricsProvider::ProvideIndependentMetrics(
    base::OnceCallback<void(bool)> done_callback,
    metrics::ChromeUserMetricsExtension* uma_proto,
    base::HistogramSnapshotManager* snapshot_manager) {
  auto* tracing_manager = content::BackgroundTracingManager::GetInstance();
  auto serialized_trace = tracing_manager->GetLatestTraceToUpload();
  if (serialized_trace.empty()) {
    std::move(done_callback).Run(false);
    return;
  }
  metrics::TraceLog* log = uma_proto->add_trace_log();
  log->set_raw_data(std::move(serialized_trace));

  // TODO(ssid): Find a better way to record other system profile metrics in
  // independent providers.
  variations::FieldTrialsProvider provider(nullptr, base::StringPiece());
  provider.ProvideSystemProfileMetricsWithLogCreationTime(
      base::TimeTicks(), uma_proto->mutable_system_profile());

  std::move(done_callback).Run(true);
}

}  // namespace tracing
