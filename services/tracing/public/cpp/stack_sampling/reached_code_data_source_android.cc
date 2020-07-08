// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/stack_sampling/reached_code_data_source_android.h"

#include <utility>
#include <vector>

#include "base/android/reached_addresses_bitset.h"
#include "base/android/reached_code_profiler.h"
#include "services/tracing/public/cpp/perfetto/perfetto_producer.h"
#include "third_party/perfetto/include/perfetto/tracing/data_source.h"
#include "third_party/perfetto/protos/perfetto/trace/profiling/profile_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace tracing {

// static
ReachedCodeDataSource* ReachedCodeDataSource::Get() {
  static base::NoDestructor<ReachedCodeDataSource> instance;
  return instance.get();
}

ReachedCodeDataSource::ReachedCodeDataSource()
    : DataSourceBase(mojom::kReachedCodeProfilerSourceName) {}

ReachedCodeDataSource::~ReachedCodeDataSource() {
  NOTREACHED();
}

void ReachedCodeDataSource::StartTracing(
    PerfettoProducer* producer,
    const perfetto::DataSourceConfig& data_source_config) {
  trace_writer_ =
      producer->CreateTraceWriter(data_source_config.target_buffer());
}

void ReachedCodeDataSource::StopTracing(
    base::OnceClosure stop_complete_callback) {
  if (!base::android::IsReachedCodeProfilerEnabled()) {
    std::move(stop_complete_callback).Run();
    return;
  }
  auto* bitset = base::android::ReachedAddressesBitset::GetTextBitset();
  // |bitset| is null when the build does not support code ordering.
  if (!bitset) {
    return;
  }
  std::vector<uint32_t> offsets = bitset->GetReachedOffsets();
  perfetto::TraceWriter::TracePacketHandle trace_packet =
      trace_writer_->NewTracePacket();
  // Delta encoded timestamps and interned data require incremental state.
  auto* streaming_profile_packet = trace_packet->set_streaming_profile_packet();
  for (uint32_t offset : offsets) {
    // TODO(ssid): add a new packed field to the trace packet proto.
    streaming_profile_packet->add_callstack_iid(offset);
  }
  trace_packet->Finalize();
  trace_writer_.reset();
  std::move(stop_complete_callback).Run();
}

void ReachedCodeDataSource::Flush(
    base::RepeatingClosure flush_complete_callback) {
  flush_complete_callback.Run();
}

void ReachedCodeDataSource::ClearIncrementalState() {}

}  // namespace tracing
