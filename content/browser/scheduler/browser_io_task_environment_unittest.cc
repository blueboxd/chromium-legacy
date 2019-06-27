// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_io_task_environment.h"

#include <memory>

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "content/browser/scheduler/browser_task_queues.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

TEST(BrowserIOTaskEnvironmentTest, CanPostTasksToThread) {
  base::Thread thread("my_thread");

  auto env = std::make_unique<BrowserIOTaskEnvironment>();
  auto handle = env->CreateHandle();
  handle->EnableAllQueues();

  base::Thread::Options options;
  options.task_environment = env.release();
  thread.StartWithOptions(options);

  auto runner =
      handle->GetBrowserTaskRunner(BrowserTaskQueues::QueueType::kDefault);

  base::WaitableEvent event;
  runner->PostTask(FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                             base::Unretained(&event)));
  event.Wait();
}

TEST(BrowserIOTaskEnvironmentTest, DefaultTaskRunnerIsAllwaysActive) {
  base::Thread thread("my_thread");

  auto env = std::make_unique<BrowserIOTaskEnvironment>();
  auto task_runner = env->GetDefaultTaskRunner();

  base::Thread::Options options;
  options.task_environment = env.release();
  thread.StartWithOptions(options);

  base::WaitableEvent event;
  task_runner->PostTask(FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                                  base::Unretained(&event)));
  event.Wait();
}

}  // namespace
}  // namespace content
