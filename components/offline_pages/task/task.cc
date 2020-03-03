// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/task/task.h"

#include <utility>

#include "base/logging.h"

namespace offline_pages {

Task::Task() = default;
Task::~Task() {
  // This may happen when tearing-down the |TaskQueue|.
  DLOG_IF(WARNING, started_ && !completed_)
      << "Task being destroyed before completion";
}

void Task::Execute(base::OnceClosure complete_callback) {
  DCHECK(!started_);
  started_ = true;
  task_completion_callback_ = std::move(complete_callback);
  Run();
}

void Task::TaskComplete() {
  DCHECK(started_);
  DCHECK(!completed_);
  completed_ = true;
  if (!task_completion_callback_.is_null())
    std::move(task_completion_callback_).Run();
}

}  // namespace offline_pages
