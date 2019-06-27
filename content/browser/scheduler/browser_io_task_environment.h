// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_BROWSER_IO_TASK_ENVIRONMENT_H_
#define CONTENT_BROWSER_SCHEDULER_BROWSER_IO_TASK_ENVIRONMENT_H_

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/threading/thread.h"
#include "content/browser/scheduler/browser_task_queues.h"
#include "content/common/content_export.h"

namespace base {
class SingleThreadTaskRunner;

namespace sequence_manager {
class SequenceManager;
}  // namespace sequence_manager

}  // namespace base

namespace content {

// TaskEnvironment for the IO thread.
class CONTENT_EXPORT BrowserIOTaskEnvironment
    : public base::Thread::TaskEnvironment {
 public:
  using Handle = BrowserTaskQueues::Handle;

  static std::unique_ptr<BrowserIOTaskEnvironment> CreateForTesting(
      base::sequence_manager::SequenceManager* sequence_manager) {
    return base::WrapUnique(new BrowserIOTaskEnvironment(sequence_manager));
  }

  BrowserIOTaskEnvironment();
  ~BrowserIOTaskEnvironment() override;

  scoped_refptr<base::SingleThreadTaskRunner> GetDefaultTaskRunner() override;
  void BindToCurrentThread(base::TimerSlack timer_slack) override;

  bool allow_blocking_for_testing() const {
    return allow_blocking_for_testing_;
  }

  // Call this before handing this over to a base::Thread to allow blocking in
  // tests.
  void SetAllowBlockingForTesting() { allow_blocking_for_testing_ = true; }

  scoped_refptr<Handle> CreateHandle() { return task_queues_->GetHandle(); }

 private:
  explicit BrowserIOTaskEnvironment(
      base::sequence_manager::SequenceManager* sequence_manager);

  // Performs the actual initialization of all the members that require a
  // SequenceManager. Just a convenience method to avoid code duplication as in
  // testing |sequence_manager_| will be null;
  void Init(base::sequence_manager::SequenceManager* sequence_manager);

  bool allow_blocking_for_testing_ = false;
  // Owned SequenceManager, null if instance created via CreateForTesting.
  std::unique_ptr<base::sequence_manager::SequenceManager> sequence_manager_;

  std::unique_ptr<BrowserTaskQueues> task_queues_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_BROWSER_IO_TASK_ENVIRONMENT_H_
