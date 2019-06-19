// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SUB_TASK_ATTRIBUTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SUB_TASK_ATTRIBUTION_H_

#include <memory>

#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class SubTaskAttribution {
  USING_FAST_MALLOC(SubTaskAttribution);

 public:
  using EntriesVector = Vector<std::unique_ptr<SubTaskAttribution>>;

  SubTaskAttribution(const AtomicString& sub_task_name,
                     const String& script_url,
                     base::TimeTicks start_time,
                     base::TimeDelta duration);
  inline AtomicString subTaskName() const { return sub_task_name_; }
  inline String scriptURL() const { return script_url_; }
  inline base::TimeTicks startTime() const { return start_time_; }
  inline base::TimeDelta duration() const { return duration_; }

  inline DOMHighResTimeStamp highResStartTime() const {
    return high_res_start_time_;
  }
  inline DOMHighResTimeStamp highResDuration() const {
    return high_res_duration_;
  }
  void setHighResStartTime(DOMHighResTimeStamp high_res_start_time) {
    high_res_start_time_ = high_res_start_time;
  }
  void setHighResDuration(DOMHighResTimeStamp high_res_duration) {
    high_res_duration_ = high_res_duration;
  }

 private:
  AtomicString sub_task_name_;
  String script_url_;
  base::TimeTicks start_time_;
  base::TimeDelta duration_;
  DOMHighResTimeStamp high_res_start_time_;
  DOMHighResTimeStamp high_res_duration_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SUB_TASK_ATTRIBUTION_H_
