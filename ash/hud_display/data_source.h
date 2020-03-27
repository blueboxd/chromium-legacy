// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_DATA_SOURCE_H_
#define ASH_HUD_DISPLAY_DATA_SOURCE_H_

#include <cstdint>

#include <limits>

namespace ash {
namespace hud_display {

// This is source of data to draw the HUD display.
class DataSource {
 public:
  struct Snapshot {
    Snapshot();
    Snapshot(const Snapshot&);

    // All memory sizes are in bytes.
    // Separate non-zero-initialized members from zero-initialized.
    int64_t free_ram{std::numeric_limits<int64_t>::max()};

    int64_t physical_ram = 0;  // Amount of physical RAM installed.
    int64_t total_ram = 0;     // As reported in /proc/meminfo.

    // Amount of RSS Private memory used by "/android" cgroup.
    int64_t arc_rss = 0;
    // Amount of RSS Shared memory used by "/android" cgroup.
    int64_t arc_rss_shared = 0;
    // Amount of RSS Private memory used by Chrome browser process.
    int64_t browser_rss = 0;
    // Amount of RSS Shared memory used by Chrome browser process.
    int64_t browser_rss_shared = 0;
    // Amount of GPU memory used by kernel GPU driver.
    int64_t gpu_kernel = 0;
    // Amount of RSS Private memory used by Chrome GPU process.
    int64_t gpu_rss = 0;
    // Amount of RSS Shared memory used by Chrome GPU process.
    int64_t gpu_rss_shared = 0;
    // Amount of RSS Private memory used by Chrome type=renderer processes.
    int64_t renderers_rss = 0;
    // Amount of RSS Shared memory used by Chrome type=renderer processes.
    int64_t renderers_rss_shared = 0;
  };

  DataSource();
  ~DataSource();

  DataSource(const DataSource&) = delete;
  DataSource& operator=(const DataSource&) = delete;

  Snapshot GetSnapshotAndReset();

 private:
  void Refresh();

  Snapshot GetSnapshot() const;
  void ResetCounters();

  // Current system snapshot.
  Snapshot snapshot_;
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_DATA_SOURCE_H_
