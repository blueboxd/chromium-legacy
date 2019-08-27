// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/job_task_source.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/task/thread_pool/test_utils.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

// Verifies the normal flow of running 2 tasks one after the other.
TEST(ThreadPoolJobTaskSourceTest, RunTasks) {
  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      DoNothing(), /* num_tasks_to_run */ 2);
  scoped_refptr<JobTaskSource> task_source = job_task->GetJobTaskSource(
      FROM_HERE, {ThreadPool(), TaskPriority::BEST_EFFORT});
  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(task_source);

  EXPECT_EQ(2U, task_source->GetRemainingConcurrency());
  {
    EXPECT_EQ(registered_task_source.WillRunTask(),
              TaskSource::RunStatus::kAllowedNotSaturated);

    auto task = registered_task_source.TakeTask();
    std::move(task->task).Run();
    EXPECT_TRUE(registered_task_source.DidProcessTask());
  }
  {
    EXPECT_EQ(registered_task_source.WillRunTask(),
              TaskSource::RunStatus::kAllowedSaturated);

    // An attempt to run an additional task is not allowed.
    EXPECT_EQ(RegisteredTaskSource::CreateForTesting(task_source).WillRunTask(),
              TaskSource::RunStatus::kDisallowed);
    EXPECT_EQ(0U, task_source->GetRemainingConcurrency());
    auto task = registered_task_source.TakeTask();
    EXPECT_EQ(RegisteredTaskSource::CreateForTesting(task_source).WillRunTask(),
              TaskSource::RunStatus::kDisallowed);

    std::move(task->task).Run();
    EXPECT_EQ(0U, task_source->GetRemainingConcurrency());
    // Returns false because the task source is out of tasks.
    EXPECT_FALSE(registered_task_source.DidProcessTask());
  }
}

// Verifies that a job task source doesn't allow any new RunStatus after Clear()
// is called.
TEST(ThreadPoolJobTaskSourceTest, Clear) {
  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      DoNothing(), /* num_tasks_to_run */ 5);
  scoped_refptr<JobTaskSource> task_source = job_task->GetJobTaskSource(
      FROM_HERE, {ThreadPool(), TaskPriority::BEST_EFFORT});

  EXPECT_EQ(5U, task_source->GetRemainingConcurrency());
  auto registered_task_source_a =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_a.WillRunTask(),
            TaskSource::RunStatus::kAllowedNotSaturated);
  auto task_a = registered_task_source_a.TakeTask();

  auto registered_task_source_b =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_b.WillRunTask(),
            TaskSource::RunStatus::kAllowedNotSaturated);

  auto registered_task_source_c =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_c.WillRunTask(),
            TaskSource::RunStatus::kAllowedNotSaturated);

  auto registered_task_source_d =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_d.WillRunTask(),
            TaskSource::RunStatus::kAllowedNotSaturated);

  {
    EXPECT_EQ(1U, task_source->GetRemainingConcurrency());
    auto task = registered_task_source_c.Clear();
    std::move(task->task).Run();
    EXPECT_EQ(0U, task_source->GetRemainingConcurrency());
  }
  // The task source shouldn't allow any further tasks after Clear.
  EXPECT_EQ(RegisteredTaskSource::CreateForTesting(task_source).WillRunTask(),
            TaskSource::RunStatus::kDisallowed);

  // Another outstanding RunStatus can still call Clear.
  {
    auto task = registered_task_source_d.Clear();
    std::move(task->task).Run();
    EXPECT_EQ(0U, task_source->GetRemainingConcurrency());
  }

  // A task that was already acquired can still run.
  std::move(task_a->task).Run();
  registered_task_source_a.DidProcessTask();

  // A valid outstanding RunStatus can also take & run a task.
  {
    auto task = registered_task_source_b.TakeTask();
    std::move(task->task).Run();
    registered_task_source_b.DidProcessTask();
  }
  // Sanity check.
  EXPECT_EQ(RegisteredTaskSource::CreateForTesting(task_source).WillRunTask(),
            TaskSource::RunStatus::kDisallowed);
}

// Verifies that multiple tasks can run in parallel up to |max_concurrency|.
TEST(ThreadPoolJobTaskSourceTest, RunTasksInParallel) {
  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      DoNothing(), /* num_tasks_to_run */ 2);
  scoped_refptr<JobTaskSource> task_source = job_task->GetJobTaskSource(
      FROM_HERE, {ThreadPool(), TaskPriority::BEST_EFFORT});

  auto registered_task_source_a =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_a.WillRunTask(),
            TaskSource::RunStatus::kAllowedNotSaturated);
  auto task_a = registered_task_source_a.TakeTask();

  auto registered_task_source_b =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_b.WillRunTask(),
            TaskSource::RunStatus::kAllowedSaturated);
  auto task_b = registered_task_source_b.TakeTask();

  // WillRunTask() should return a null RunStatus once the max concurrency is
  // reached.
  EXPECT_EQ(RegisteredTaskSource::CreateForTesting(task_source).WillRunTask(),
            TaskSource::RunStatus::kDisallowed);

  std::move(task_a->task).Run();
  // Adding a task before closing the first run operation should cause the task
  // source to re-enqueue.
  job_task->SetNumTasksToRun(2);
  EXPECT_TRUE(registered_task_source_a.DidProcessTask());

  std::move(task_b->task).Run();
  EXPECT_TRUE(registered_task_source_b.DidProcessTask());

  auto registered_task_source_c =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_c.WillRunTask(),
            TaskSource::RunStatus::kAllowedSaturated);
  auto task_c = registered_task_source_c.TakeTask();

  std::move(task_c->task).Run();
  EXPECT_FALSE(registered_task_source_c.DidProcessTask());
}

TEST(ThreadPoolJobTaskSourceTest, InvalidTakeTask) {
  auto job_task =
      base::MakeRefCounted<test::MockJobTask>(DoNothing(),
                                              /* num_tasks_to_run */ 1);
  scoped_refptr<JobTaskSource> task_source = job_task->GetJobTaskSource(
      FROM_HERE, {ThreadPool(), TaskPriority::BEST_EFFORT});

  auto registered_task_source_a =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_a.WillRunTask(),
            TaskSource::RunStatus::kAllowedSaturated);

  auto registered_task_source_b =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_b.WillRunTask(),
            TaskSource::RunStatus::kDisallowed);

  // Can not be called with an invalid RunStatus.
  EXPECT_DCHECK_DEATH({ auto task = registered_task_source_b.TakeTask(); });

  auto task = registered_task_source_a.TakeTask();
  registered_task_source_a.DidProcessTask();
}

TEST(ThreadPoolJobTaskSourceTest, InvalidDidProcessTask) {
  auto job_task =
      base::MakeRefCounted<test::MockJobTask>(DoNothing(),
                                              /* num_tasks_to_run */ 1);
  scoped_refptr<JobTaskSource> task_source = job_task->GetJobTaskSource(
      FROM_HERE, {ThreadPool(), TaskPriority::BEST_EFFORT});

  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source.WillRunTask(),
            TaskSource::RunStatus::kAllowedSaturated);
  // Can not be called before TakeTask().
  EXPECT_DCHECK_DEATH(registered_task_source.DidProcessTask());

  auto task = registered_task_source.TakeTask();
  registered_task_source.DidProcessTask();
}

}  // namespace internal
}  // namespace base
