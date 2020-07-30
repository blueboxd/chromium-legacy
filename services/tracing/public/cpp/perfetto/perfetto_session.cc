// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/perfetto_session.h"

#include "third_party/perfetto/protos/perfetto/common/trace_stats.gen.h"

namespace tracing {

double GetTraceBufferUsage(const perfetto::protos::gen::TraceStats& stats) {
  // Chrome always uses a single tracing buffer.
  if (stats.buffer_stats_size() != 1u)
    return 0.f;
  const perfetto::protos::gen::TraceStats::BufferStats& buf_stats =
      stats.buffer_stats()[0];
  size_t bytes_in_buffer = buf_stats.bytes_written() - buf_stats.bytes_read() -
                           buf_stats.bytes_overwritten() +
                           buf_stats.padding_bytes_written() -
                           buf_stats.padding_bytes_cleared();
  return bytes_in_buffer / static_cast<double>(buf_stats.buffer_size());
}

bool HasLostData(const perfetto::protos::gen::TraceStats& stats) {
  // Chrome always uses a single tracing buffer.
  if (stats.buffer_stats_size() != 1u)
    return false;
  const perfetto::protos::gen::TraceStats::BufferStats& buf_stats =
      stats.buffer_stats()[0];
  return buf_stats.chunks_overwritten() > 0 ||
         buf_stats.chunks_discarded() > 0 || buf_stats.abi_violations() > 0 ||
         buf_stats.trace_writer_packet_loss() > 0;
}

}  // namespace tracing