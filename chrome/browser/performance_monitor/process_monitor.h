// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_MONITOR_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_MONITOR_H_

#include <map>
#include <memory>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/timer/timer.h"
#include "chrome/browser/performance_monitor/process_metrics_history.h"

namespace performance_monitor {

class ProcessMetricsHistory;

// ProcessMonitor is a tool which periodically monitors performance metrics
// of all the Chrome processes for histogram logging and possibly taking action
// upon noticing serious performance degradation.
//
// TODO(sebmarchand): Make this class not a singleton.
class ProcessMonitor {
 public:
  // Returns the current ProcessMonitor instance if one exists; otherwise
  // constructs a new ProcessMonitor.
  static ProcessMonitor* GetInstance();

  // Start the cycle of metrics gathering.
  void StartGatherCycle();

 private:
  friend struct base::LazyInstanceTraitsBase<ProcessMonitor>;

  using MetricsMap =
      std::map<base::ProcessHandle, std::unique_ptr<ProcessMetricsHistory>>;

  ProcessMonitor();
  ~ProcessMonitor();

  // Mark the given process as alive in the current update iteration.
  // This means adding an entry to the map of watched processes if it's not
  // already present.
  void MarkProcessAsAlive(const ProcessMetricsMetadata& process_data,
                          int current_update_sequence);

  // Updates the ProcessMetrics map with the current list of processes and
  // gathers metrics from each entry.
  void GatherMetricsMapOnUIThread();
  void GatherMetricsMapOnIOThread(int current_update_sequence);

  // A map of currently running ProcessHandles to ProcessMetrics.
  MetricsMap metrics_map_;

  // The timer to signal ProcessMonitor to perform its timed collections.
  base::RepeatingTimer repeating_timer_;

  DISALLOW_COPY_AND_ASSIGN(ProcessMonitor);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_MONITOR_H_
