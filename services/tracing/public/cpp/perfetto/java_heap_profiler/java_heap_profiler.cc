// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/java_heap_profiler/java_heap_profiler.h"

#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"

namespace tracing {

JavaHeapProfiler::JavaHeapProfiler()
    : DataSourceBase(mojom::kJavaHeapProfilerSourceName) {}

// static
JavaHeapProfiler* JavaHeapProfiler::GetInstance() {
  static base::NoDestructor<JavaHeapProfiler> instance;
  return instance.get();
}

void JavaHeapProfiler::StartTracing(
    PerfettoProducer* producer,
    const perfetto::DataSourceConfig& data_source_config) {}

void JavaHeapProfiler::StopTracing(base::OnceClosure stop_complete_callback) {
  producer_ = nullptr;
  std::move(stop_complete_callback).Run();
}

void JavaHeapProfiler::Flush(base::RepeatingClosure flush_complete_callback) {
  flush_complete_callback.Run();
}
}  // namespace tracing
