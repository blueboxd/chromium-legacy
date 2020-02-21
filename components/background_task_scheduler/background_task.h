// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACKGROUND_TASK_SCHEDULER_BACKGROUND_TASK_H_
#define COMPONENTS_BACKGROUND_TASK_SCHEDULER_BACKGROUND_TASK_H_

#include "base/callback.h"
#include "base/macros.h"
#include "components/background_task_scheduler/task_parameters.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "content/public/browser/browser_context.h"

namespace background_task {

using TaskFinishedCallback = base::OnceCallback<void(bool)>;

// Entry point for callbacks from BackgroundTaskScheduler. Any classes
// implementing this interface must have a public constructor which takes no
// arguments. The callback will be executed on the main thread.
class BackgroundTask {
 public:
  // The following two methods represent the callback from
  // BackgroundTaskScheduler when your task should start processing. It is
  // invoked on the main thread, and if your task finishes quickly, you should
  // return false from this method when you are done processing. If this is a
  // long-running task, you should return true from this method, and instead
  // invoke the |callback| when the processing is finished on some other thread.
  // While this method is running the system holds a wakelock. If false is
  // returned from this method, the wakelock is immediately released, but if
  // this method returns true, the wakelock is not released until either the
  // |callback| is invoked, or the system calls onStopTask. Depending on whether
  // Chrome is running in service manager only mode or full browser mode, one or
  // both of the following two methods are invoked.

  // Callback invoked when chrome is running in service manager only mode. User
  // can start executing the task here or save the params and wait till full
  // browser is started and OnFullBrowserLoaded is invoked.
  virtual void OnStartTaskInReducedMode(const TaskParameters& task_params,
                                        TaskFinishedCallback callback,
                                        SimpleFactoryKey* key) {}

  // Callback invoked when Chrome is running in full browser mode. This is
  // invoked only if the chrome was started in reduced mode.
  virtual void OnStartTaskWithFullBrowser(
      const TaskParameters& task_params,
      TaskFinishedCallback callback,
      content::BrowserContext* browser_context) {}

  // Callback invoked whenever the full browser starts after starting first in
  // service manager only mode.
  virtual void OnFullBrowserLoaded(content::BrowserContext* browser_context) {}

  // Callback from BackgroundTaskScheduler when the system has determined that
  // the execution of the task must stop immediately, even before the
  // TaskFinishedCallback has been invoked. This will typically happen whenever
  // the required conditions for the task are no longer met. See TaskInfo for
  // more details. A wakelock is held by the system while this callback is
  // invoked, and immediately released after this method returns.
  virtual bool OnStopTask(const TaskParameters& task_params) = 0;

  // Destructor.
  virtual ~BackgroundTask() {}

 protected:
  BackgroundTask() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundTask);
};

}  // namespace background_task

#endif  // COMPONENTS_BACKGROUND_TASK_SCHEDULER_BACKGROUND_TASK_H_
