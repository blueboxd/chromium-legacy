// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/task_queue_web_view.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/trace_event/trace_event.h"

namespace android_webview {
base::LazyInstance<base::ThreadLocalBoolean>::DestructorAtExit
    ScopedAllowGL::allow_gl;

// static
bool ScopedAllowGL::IsAllowed() {
  return allow_gl.Get().Get();
}

ScopedAllowGL::ScopedAllowGL() {
  DCHECK(!allow_gl.Get().Get());
  allow_gl.Get().Set(true);

  TaskQueueWebView* service = TaskQueueWebView::GetInstance();
  DCHECK(service);
}

ScopedAllowGL::~ScopedAllowGL() {
  TaskQueueWebView* service = TaskQueueWebView::GetInstance();
  DCHECK(service);
  service->RunAllTasks();
  allow_gl.Get().Set(false);
}

// static
TaskQueueWebView* TaskQueueWebView::GetInstance() {
  static TaskQueueWebView* task_queue =
      TaskQueueWebView::CreateTaskQueueWebView();
  return task_queue;
}

// static
TaskQueueWebView* TaskQueueWebView::CreateTaskQueueWebView() {
  return new TaskQueueWebView();
}

TaskQueueWebView::TaskQueueWebView() {
  DETACH_FROM_THREAD(task_queue_thread_checker_);
}

TaskQueueWebView::~TaskQueueWebView() {
  DCHECK(tasks_.empty());
}

void TaskQueueWebView::ScheduleTask(base::OnceClosure task, bool out_of_order) {
  DCHECK_CALLED_ON_VALID_THREAD(task_queue_thread_checker_);
  LOG_IF(FATAL, !ScopedAllowGL::IsAllowed())
      << "ScheduleTask outside of ScopedAllowGL";
  if (out_of_order)
    tasks_.emplace_front(std::move(task));
  else
    tasks_.emplace_back(std::move(task));
  RunTasks();
}

void TaskQueueWebView::ScheduleIdleTask(base::OnceClosure task) {
  LOG_IF(FATAL, !ScopedAllowGL::IsAllowed())
      << "ScheduleDelayedWork outside of ScopedAllowGL";
  DCHECK_CALLED_ON_VALID_THREAD(task_queue_thread_checker_);
  idle_tasks_.push(std::make_pair(base::Time::Now(), std::move(task)));
}

void TaskQueueWebView::ScheduleClientTask(base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_THREAD(task_queue_thread_checker_);
  client_tasks_.emplace(std::move(task));
}

void TaskQueueWebView::RunTasks() {
  TRACE_EVENT0("android_webview", "TaskQueueWebView::RunTasks");
  DCHECK_CALLED_ON_VALID_THREAD(task_queue_thread_checker_);
  if (inside_run_tasks_)
    return;
  base::AutoReset<bool> inside(&inside_run_tasks_, true);
  while (tasks_.size()) {
    std::move(tasks_.front()).Run();
    tasks_.pop_front();
  }
}

void TaskQueueWebView::RunAllTasks() {
  DCHECK_CALLED_ON_VALID_THREAD(task_queue_thread_checker_);
  RunTasks();
  PerformAllIdleWork();
  DCHECK(tasks_.empty());
  DCHECK(idle_tasks_.empty());

  // Client tasks may generate more service tasks, so run this
  // in a loop.
  while (!client_tasks_.empty()) {
    base::queue<base::OnceClosure> local_client_tasks;
    local_client_tasks.swap(client_tasks_);
    while (!local_client_tasks.empty()) {
      std::move(local_client_tasks.front()).Run();
      local_client_tasks.pop();
    }

    RunTasks();
    PerformAllIdleWork();
    DCHECK(tasks_.empty());
    DCHECK(idle_tasks_.empty());
  }
}

void TaskQueueWebView::PerformAllIdleWork() {
  TRACE_EVENT0("android_webview", "TaskQueuewebview::PerformAllIdleWork");
  DCHECK_CALLED_ON_VALID_THREAD(task_queue_thread_checker_);
  if (inside_run_idle_tasks_)
    return;
  base::AutoReset<bool> inside(&inside_run_idle_tasks_, true);
  while (idle_tasks_.size() > 0) {
    base::OnceClosure task = std::move(idle_tasks_.front().second);
    idle_tasks_.pop();
    std::move(task).Run();
  }
}

}  // namespace android_webview
