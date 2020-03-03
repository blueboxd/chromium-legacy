// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_EVENT_METRICS_H_
#define CC_METRICS_EVENT_METRICS_H_

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "ui/events/types/event_type.h"

namespace cc {

// Data about an event useful in generating event latency metrics.
class CC_EXPORT EventMetrics {
 public:
  EventMetrics(ui::EventType type, base::TimeTicks time_stamp);

  const char* GetTypeName() const;
  bool IsWhitelisted() const;

  ui::EventType type() const { return type_; }
  base::TimeTicks time_stamp() const { return time_stamp_; }

  bool operator==(const EventMetrics& other) const;

 private:
  ui::EventType type_;
  base::TimeTicks time_stamp_;
};

}  // namespace cc

#endif  // CC_METRICS_EVENT_METRICS_H_
