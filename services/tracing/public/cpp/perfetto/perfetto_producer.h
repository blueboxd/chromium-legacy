// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_PRODUCER_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_PRODUCER_H_

#include <memory>

#include "base/component_export.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/basic_types.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/tracing_service.h"

namespace perfetto {
class SharedMemoryArbiter;
}  // namespace perfetto

namespace tracing {

// This class represents the perfetto producer endpoint which is used for
// producers to talk to the Perfetto service. It also provides methods to
// interact with the shared memory buffer by binding and creating TraceWriters.
//
// In addition to the PerfettoProducers' pure virtual methods, subclasses must
// implement the remaining methods of the ProducerEndpoint interface.
class COMPONENT_EXPORT(TRACING_CPP) PerfettoProducer {
 public:
  PerfettoProducer(PerfettoTaskRunner* task_runner);

  virtual ~PerfettoProducer();

  // Setup the shared memory buffer for startup tracing. Returns false on
  // failure.
  virtual bool SetupStartupTracing() = 0;

  // See SharedMemoryArbiter::CreateStartupTraceWriter.
  std::unique_ptr<perfetto::TraceWriter> CreateStartupTraceWriter(
      uint32_t startup_session_id);

  // See SharedMemoryArbiter::BindStartupTargetBuffer. Should be called on the
  // producer's task runner.
  virtual void BindStartupTargetBuffer(
      uint32_t startup_session_id,
      perfetto::BufferID startup_target_buffer);

  // Used by the DataSource implementations to create TraceWriters
  // for writing their protobufs, and respond to flushes.
  //
  // Should only be called while a tracing session is active and a
  // SharedMemoryArbiter exists.
  //
  // Virtual for testing.
  virtual std::unique_ptr<perfetto::TraceWriter> CreateTraceWriter(
      perfetto::BufferID target_buffer,
      perfetto::BufferExhaustedPolicy =
          perfetto::BufferExhaustedPolicy::kDefault);

  // Returns the SharedMemoryArbiter if available.
  // TODO(eseckler): Once startup tracing v2 is available in Chrome, this could
  // become GetSharedMemoryArbiter() instead.
  virtual perfetto::SharedMemoryArbiter* MaybeSharedMemoryArbiter() = 0;

  // Informs the PerfettoProducer a new Data Source was added. This instance
  // will also be found in |data_sources| having just be inserted before this
  // method is called by PerfettoTracedProcess. This enables the
  // PerfettoProducer to perform initialization on new data sources.
  virtual void NewDataSourceAdded(
      const PerfettoTracedProcess::DataSourceBase* const data_source) = 0;

  // Returns true if this PerfettoProducer is currently tracing.
  virtual bool IsTracingActive() = 0;

  static void DeleteSoonForTesting(
      std::unique_ptr<PerfettoProducer> perfetto_producer);

 protected:
  friend class MockProducerHost;

  // TODO(oysteine): Find a good compromise between performance and
  // data granularity (mainly relevant to running with small buffer sizes
  // when we use background tracing) on Android.
#if defined(OS_ANDROID)
  static constexpr size_t kSMBPageSizeBytes = 4 * 1024;
#else
  static constexpr size_t kSMBPageSizeBytes = 32 * 1024;
#endif

  // TODO(oysteine): Figure out a good buffer size.
  static constexpr size_t kSMBSizeBytes = 4 * 1024 * 1024;

  PerfettoTaskRunner* task_runner();

 private:
  PerfettoTaskRunner* const task_runner_;
};
}  // namespace tracing
#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_PRODUCER_H_
