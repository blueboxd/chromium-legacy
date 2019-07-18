// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SCHEDULER_SETTINGS_H_
#define CC_SCHEDULER_SCHEDULER_SETTINGS_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/cc_export.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace cc {

class CC_EXPORT SchedulerSettings {
 public:
  SchedulerSettings();
  SchedulerSettings(const SchedulerSettings& other);
  ~SchedulerSettings();

  // Whether a BeginMainFrame should be issued while there is a pending-tree
  // still waiting to be activated. This is disabled by default for the UI
  // compositor, and enabled for renderers (unless there are too few cores).
  bool main_frame_before_activation_enabled = false;

  bool commit_to_active_tree = false;
  bool using_synchronous_renderer_compositor = false;
  bool enable_latency_recovery = true;
  bool wait_for_all_pipeline_stages_before_draw = false;

  int maximum_number_of_failed_draws_before_draw_is_forced = 3;

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValue() const;
};

}  // namespace cc

#endif  // CC_SCHEDULER_SCHEDULER_SETTINGS_H_
