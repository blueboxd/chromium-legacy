// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_TASK_QUEUE_WEB_VIEW_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_TASK_QUEUE_WEB_VIEW_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/containers/queue.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_local.h"
#include "base/time/time.h"

namespace android_webview {

// This class is used to control when to access GL.
class ScopedAllowGL {
 public:
  ScopedAllowGL();
  ~ScopedAllowGL();

  static bool IsAllowed();

 private:
  static base::LazyInstance<base::ThreadLocalBoolean>::DestructorAtExit
      allow_gl;

  DISALLOW_COPY_AND_ASSIGN(ScopedAllowGL);
};

// In WebView, there is a single task queue that runs all tasks instead of
// thread task runners. This is the class actually scheduling and running tasks
// for WebView. This is used by both CommandBuffer and SkiaDDL.
class TaskQueueWebView {
 public:
  // Static method that makes sure this is only one copy of this class.
  static TaskQueueWebView* GetInstance();
  ~TaskQueueWebView();

  // Called by TaskForwardingSequence. |out_of_order| indicates if task should
  // be run ahead of already enqueued tasks.
  void ScheduleTask(base::OnceClosure task, bool out_of_order);

  // Called by DeferredGpuCommandService to schedule delayed tasks.
  void ScheduleIdleTask(base::OnceClosure task);

  // Called by both DeferredGpuCommandService and
  // SkiaOutputSurfaceDisplayContext to post task to client thread.
  void ScheduleClientTask(base::OnceClosure task);

  // Called by ScopedAllowGL and ScheduleTask().
  void RunAllTasks();
  void RunTasks();

 private:
  static TaskQueueWebView* CreateTaskQueueWebView();
  TaskQueueWebView();

  // Flush the idle queue until it is empty.
  void PerformAllIdleWork();

  // All access to task queue should happen on a single thread.
  THREAD_CHECKER(task_queue_thread_checker_);
  base::circular_deque<base::OnceClosure> tasks_;
  base::queue<std::pair<base::Time, base::OnceClosure>> idle_tasks_;
  base::queue<base::OnceClosure> client_tasks_;

  bool inside_run_tasks_ = false;
  bool inside_run_idle_tasks_ = false;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueWebView);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_TASK_QUEUE_WEB_VIEW_H_
