// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/data_source.h"

#include <algorithm>

#include "ash/hud_display/memory_status.h"
#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"

namespace ash {
namespace hud_display {
namespace {

// Returns number of bytes rounded up to next Gigabyte.
int64_t EstimatePhysicalRAMSize(int64_t total_ram) {
  // Round up to nearest Gigabyte.
  constexpr int64_t one_gig = 1024 * 1024 * 1024;
  if (total_ram % one_gig) {
    return ((total_ram / one_gig) + 1) * one_gig;
  }
  return total_ram;
}
}  // anonymous namespace

// --------------------------------

////////////////////////////////////////////////////////////////////////////////
// DataSource, public:

DataSource::Snapshot::Snapshot() = default;
DataSource::Snapshot::Snapshot(const Snapshot&) = default;

DataSource::DataSource() = default;
DataSource::~DataSource() = default;

DataSource::Snapshot DataSource::GetSnapshotAndReset() {
  // Refresh data synchronously.
  Refresh();

  Snapshot snapshot = GetSnapshot();
  ResetCounters();
  return snapshot;
}

DataSource::Snapshot DataSource::GetSnapshot() const {
  return snapshot_;
}

void DataSource::ResetCounters() {
  snapshot_ = Snapshot();
}

////////////////////////////////////////////////////////////////////////////////
// DataSource, private:

void DataSource::Refresh() {
  const MemoryStatus memory_status;

  snapshot_.physical_ram =
      std::max(snapshot_.physical_ram,
               EstimatePhysicalRAMSize(memory_status.total_ram_size()));
  snapshot_.total_ram =
      std::max(snapshot_.total_ram, memory_status.total_ram_size());
  snapshot_.free_ram = std::min(snapshot_.free_ram, memory_status.total_free());
  snapshot_.arc_rss = std::max(snapshot_.arc_rss, memory_status.arc_rss());
  snapshot_.arc_rss_shared =
      std::max(snapshot_.arc_rss_shared, memory_status.arc_rss_shared());
  snapshot_.browser_rss =
      std::max(snapshot_.browser_rss, memory_status.browser_rss());
  snapshot_.browser_rss_shared = std::max(snapshot_.browser_rss_shared,
                                          memory_status.browser_rss_shared());
  snapshot_.renderers_rss =
      std::max(snapshot_.renderers_rss, memory_status.renderers_rss());
  snapshot_.renderers_rss_shared = std::max(
      snapshot_.renderers_rss_shared, memory_status.renderers_rss_shared());
  snapshot_.gpu_rss_shared =
      std::max(snapshot_.gpu_rss_shared, memory_status.gpu_rss_shared());
  snapshot_.gpu_rss = std::max(snapshot_.gpu_rss, memory_status.gpu_rss());
  snapshot_.gpu_kernel =
      std::max(snapshot_.gpu_kernel, memory_status.gpu_kernel());
}

}  // namespace hud_display
}  // namespace ash
