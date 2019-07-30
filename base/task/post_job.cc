// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/post_job.h"

#include "base/task/thread_pool/job_task_source.h"

namespace base {
namespace experimental {

JobDelegate::JobDelegate(internal::JobTaskSource* task_source)
    : task_source_(task_source) {}

bool JobDelegate::ShouldYield() {
  // TODO(crbug.com/839091): Implement this.
  return false;
}

void JobDelegate::YieldIfNeeded() {
  // TODO(crbug.com/839091): Implement this.
}

void JobDelegate::NotifyConcurrencyIncrease() {
  task_source_->NotifyConcurrencyIncrease();
}

}  // namespace experimental
}  // namespace base