// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_task_executor.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/thread_pool.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/scheduler/browser_io_task_environment.h"
#include "content/browser/scheduler/browser_task_queues.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using ::base::TaskPriority;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::SizeIs;

using QueueType = BrowserTaskQueues::QueueType;

class BrowserTaskExecutorTest : public testing::Test {
 private:
  TestBrowserThreadBundle thread_bundle_{
      base::test::ScopedTaskEnvironment::MainThreadType::UI,
      base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME};
};

using StrictMockTask =
    testing::StrictMock<base::MockCallback<base::RepeatingCallback<void()>>>;

TEST_F(BrowserTaskExecutorTest, RunAllPendingTasksForTestingOnUI) {
  StrictMockTask task_1;
  StrictMockTask task_2;
  EXPECT_CALL(task_1, Run).WillOnce(testing::Invoke([&]() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, task_2.Get());
  }));

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, task_1.Get());

  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::UI);

  // Cleanup pending tasks, as TestBrowserThreadBundle will run them.
  Mock::VerifyAndClearExpectations(&task_1);
  EXPECT_CALL(task_2, Run);
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);
}

TEST_F(BrowserTaskExecutorTest, RunAllPendingTasksForTestingOnIO) {
  StrictMockTask task_1;
  StrictMockTask task_2;
  EXPECT_CALL(task_1, Run).WillOnce(testing::Invoke([&]() {
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO}, task_2.Get());
  }));

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO}, task_1.Get());

  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);

  // Cleanup pending tasks, as TestBrowserThreadBundle will run them.
  Mock::VerifyAndClearExpectations(&task_1);
  EXPECT_CALL(task_2, Run);
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);
}

TEST_F(BrowserTaskExecutorTest, RunAllPendingTasksForTestingOnIOIsReentrant) {
  StrictMockTask task_1;
  StrictMockTask task_2;
  StrictMockTask task_3;

  EXPECT_CALL(task_1, Run).WillOnce(Invoke([&]() {
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO}, task_2.Get());
    BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(
        BrowserThread::IO);
  }));
  EXPECT_CALL(task_2, Run).WillOnce(Invoke([&]() {
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO}, task_3.Get());
  }));

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO}, task_1.Get());
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);

  // Cleanup pending tasks, as TestBrowserThreadBundle will run them.
  Mock::VerifyAndClearExpectations(&task_1);
  Mock::VerifyAndClearExpectations(&task_2);
  EXPECT_CALL(task_3, Run);
  BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);
}

// Helper to perform the same tets for all BrowserThread::ID values.
class BrowserTaskTraitsMappingTest : public BrowserTaskExecutorTest {
 protected:
  template <BrowserThread::ID ID>
  void CheckExpectations() {
    EXPECT_EQ(GetQueueType({ID, TaskPriority::BEST_EFFORT}),
              QueueType::kBestEffort);
    EXPECT_EQ(GetQueueType({ID, TaskPriority::USER_VISIBLE}),
              QueueType::kUserVisible);
    EXPECT_EQ(GetQueueType({ID, TaskPriority::USER_BLOCKING}),
              QueueType::kUserBlocking);

    EXPECT_EQ(GetQueueType({ID, BrowserTaskType::kBootstrap}),
              QueueType::kBootstrap);
    EXPECT_EQ(GetQueueType({ID, BrowserTaskType::kDefault}),
              QueueType::kUserBlocking);
    EXPECT_EQ(GetQueueType({ID, BrowserTaskType::kNavigation}),
              QueueType::kNavigationAndPreconnection);
    EXPECT_EQ(GetQueueType({ID, BrowserTaskType::kPreconnect}),
              QueueType::kNavigationAndPreconnection);

    EXPECT_EQ(GetQueueType({ID}), QueueType::kUserBlocking);
  }

 private:
  QueueType GetQueueType(const base::TaskTraits& traits) {
    return BrowserTaskExecutor::GetThreadIdAndQueueType(traits).queue_type;
  }
};

TEST_F(BrowserTaskTraitsMappingTest, BrowserTaskTraitsMapToProperPriorities) {
  CheckExpectations<BrowserThread::UI>();
  CheckExpectations<BrowserThread::IO>();
}

TEST_F(BrowserTaskTraitsMappingTest,
       UIThreadTaskRunnerHasSamePriorityAsUIBlocking) {
  auto ui_blocking = base::CreateSingleThreadTaskRunner(
      {BrowserThread::UI, TaskPriority::USER_BLOCKING});
  auto thread_task_runner = base::ThreadTaskRunnerHandle::Get();

  std::vector<int> order;
  ui_blocking->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { order.push_back(1); }));
  thread_task_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { order.push_back(10); }));
  ui_blocking->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { order.push_back(2); }));
  thread_task_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { order.push_back(20); }));

  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(order, ElementsAre(1, 10, 2, 20));
}

class BrowserTaskExecutorWithCustomSchedulerTest : public testing::Test {
 private:
  class ScopedTaskEnvironmentWithCustomScheduler
      : public base::test::ScopedTaskEnvironment {
   public:
    ScopedTaskEnvironmentWithCustomScheduler()
        : base::test::ScopedTaskEnvironment(
              SubclassCreatesDefaultTaskRunner{},
              base::test::ScopedTaskEnvironment::MainThreadType::UI,
              base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME) {
      std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler =
          BrowserUIThreadScheduler::CreateForTesting(sequence_manager(),
                                                     GetTimeDomain());
      DeferredInitFromSubclass(
          browser_ui_thread_scheduler->GetHandle()->GetBrowserTaskRunner(
              QueueType::kDefault));
      BrowserTaskExecutor::CreateForTesting(
          std::move(browser_ui_thread_scheduler),
          std::make_unique<BrowserIOTaskEnvironment>());
    }
  };

 public:
  using QueueType = BrowserTaskQueues::QueueType;

  ~BrowserTaskExecutorWithCustomSchedulerTest() override {
    BrowserTaskExecutor::ResetForTesting();
  }

 protected:
  ScopedTaskEnvironmentWithCustomScheduler scoped_task_environment_;
};

TEST_F(BrowserTaskExecutorWithCustomSchedulerTest,
       UserVisibleOrBlockingTasksRunDuringStartup) {
  StrictMockTask best_effort;
  StrictMockTask user_visible;
  StrictMockTask user_blocking;

  base::PostTaskWithTraits(FROM_HERE,
                           {BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
                           best_effort.Get());
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
      user_visible.Get());
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_BLOCKING},
      user_blocking.Get());

  EXPECT_CALL(user_visible, Run);
  EXPECT_CALL(user_blocking, Run);

  scoped_task_environment_.RunUntilIdle();
}

TEST_F(BrowserTaskExecutorWithCustomSchedulerTest,
       BestEffortTasksRunAfterStartup) {
  auto ui_best_effort_runner = base::CreateSingleThreadTaskRunnerWithTraits(
      {BrowserThread::UI, base::TaskPriority::BEST_EFFORT});

  StrictMockTask best_effort;

  ui_best_effort_runner->PostTask(FROM_HERE, best_effort.Get());
  ui_best_effort_runner->PostDelayedTask(
      FROM_HERE, best_effort.Get(), base::TimeDelta::FromMilliseconds(100));
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
      best_effort.Get(), base::TimeDelta::FromMilliseconds(100));
  base::PostTaskWithTraits(FROM_HERE,
                           {BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
                           best_effort.Get());
  scoped_task_environment_.RunUntilIdle();

  BrowserTaskExecutor::EnableAllQueues();
  EXPECT_CALL(best_effort, Run).Times(4);
  scoped_task_environment_.FastForwardBy(
      base::TimeDelta::FromMilliseconds(100));
}

}  // namespace content
