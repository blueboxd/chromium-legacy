// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/crash_memory_metrics_collector_android.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/bind_interface_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"

// Keys used to attach handler to the RenderProcessHost
const void* const CrashMemoryMetricsCollector::kCrashMemoryMetricsCollectorKey =
    &CrashMemoryMetricsCollector::kCrashMemoryMetricsCollectorKey;

CrashMemoryMetricsCollector*
CrashMemoryMetricsCollector::GetFromRenderProcessHost(
    content::RenderProcessHost* rph) {
  return static_cast<CrashMemoryMetricsCollector*>(rph->GetUserData(
      CrashMemoryMetricsCollector::kCrashMemoryMetricsCollectorKey));
}

CrashMemoryMetricsCollector::CrashMemoryMetricsCollector(
    content::RenderProcessHost* rph) {
  // Initialize bindings with CrashMemoryMetricsReporter.
  content::BindInterface(rph, &crash_memory_metrics_reporter_);
  // Create shared memory and pass it to crash_memory_metrics_reporter.
  base::UnsafeSharedMemoryRegion shared_metrics_buffer =
      base::UnsafeSharedMemoryRegion::Create(
          sizeof(blink::OomInterventionMetrics));
  metrics_mapping_ = shared_metrics_buffer.Map();
  memset(metrics_mapping_.memory(), 0, sizeof(blink::OomInterventionMetrics));
  mojo::Remote<blink::mojom::CrashMemoryMetricsReporter>(
      std::move(crash_memory_metrics_reporter_))
      ->SetSharedMemory(shared_metrics_buffer.Duplicate());
}

CrashMemoryMetricsCollector::~CrashMemoryMetricsCollector() = default;

const blink::OomInterventionMetrics*
CrashMemoryMetricsCollector::MemoryMetrics() {
  // This should be called after SetSharedMemory.
  DCHECK(metrics_mapping_.IsValid());
  return metrics_mapping_.GetMemoryAs<blink::OomInterventionMetrics>();
}
