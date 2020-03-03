// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_TASK_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_TASK_TASK_H_

#include "base/callback.h"

namespace offline_pages {

// A task which may run asynchronous steps. Its primary purpose is to implement
// operations to be inserted into a |TaskQueue|, however, tasks can also be run
// outside of a |TaskQueue|.
class Task {
 public:
  Task();
  virtual ~Task();
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;

  void Execute(base::OnceClosure complete_callback);

 protected:
  // Entry point to the task. Called by |Execute()| to perform the task.
  // Must call |TaskComplete()| as the final step.
  virtual void Run() = 0;

  // Tasks must call |TaskComplete()| as their last step.
  void TaskComplete();

  base::OnceClosure task_completion_callback_;
  bool completed_ = false;
  bool started_ = false;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_TASK_TASK_H_
