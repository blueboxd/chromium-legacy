// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/drive_operation_queue.h"
#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/drive/file_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

namespace {

constexpr int kDesiredQPS = 5;
constexpr base::TimeDelta kTokenRefreshRate =
    base::TimeDelta::FromSeconds(1) / kDesiredQPS;

class FakeDriveOperation {
 public:
  FakeDriveOperation() {}

  base::WeakPtr<FakeDriveOperation> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void FakeOperation(const FileOperationCallback& callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(!task_has_executed);

    task_has_executed = true;
    callback.Run(FILE_ERROR_OK);
  }

  bool HasTaskExecuted() const { return task_has_executed; }

 private:
  bool task_has_executed = false;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FakeDriveOperation> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeDriveOperation);
};

}  // namespace

class DriveOperationQueueTest : public testing::Test {
 protected:
  using OperationQueue = DriveBackgroundOperationQueue<FakeDriveOperation>;

  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::TestMockTimeTaskRunner::Type::kBoundToThread);
    operation_queue_ =
        std::make_unique<DriveBackgroundOperationQueue<FakeDriveOperation>>(
            kDesiredQPS, task_runner_->GetMockTickClock());
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  std::unique_ptr<OperationQueue> operation_queue_;
};

// Put up to |kDesiredQPS| entries in queue, all should run without the timer
// needing to advance.
TEST_F(DriveOperationQueueTest, BurstMode) {
  std::vector<std::unique_ptr<FakeDriveOperation>> operations;

  for (int i = 0; i < kDesiredQPS; ++i) {
    operations.emplace_back(std::make_unique<FakeDriveOperation>());
  }

  int callbacks_run = 0;

  for (auto& operation : operations) {
    operation_queue_->AddOperation(
        operation->GetWeakPtr(),
        base::BindOnce(&FakeDriveOperation::FakeOperation,
                       operation->GetWeakPtr()),
        base::BindLambdaForTesting(
            [&callbacks_run](drive::FileError error) { ++callbacks_run; }));
  }

  // Execute all pending tasks without advancing virtual time.
  task_runner_->RunUntilIdle();

  ASSERT_EQ(callbacks_run, kDesiredQPS);

  for (auto& operation : operations) {
    ASSERT_TRUE(operation->HasTaskExecuted());
  }
}

// Ensure that tokens are refilled as the timer advances.
TEST_F(DriveOperationQueueTest, RefillTokens) {
  constexpr int kAdditionalTasks = 5;
  constexpr int kTasksToQueue = kDesiredQPS + kAdditionalTasks;

  std::vector<std::unique_ptr<FakeDriveOperation>> operations;

  for (int i = 0; i < kTasksToQueue; ++i) {
    operations.emplace_back(std::make_unique<FakeDriveOperation>());
  }

  int callbacks_run = 0;

  for (int i = 0; i < kTasksToQueue; ++i) {
    operation_queue_->AddOperation(
        operations[i]->GetWeakPtr(),
        base::BindOnce(&FakeDriveOperation::FakeOperation,
                       operations[i]->GetWeakPtr()),
        base::BindLambdaForTesting(
            [&callbacks_run](drive::FileError) { ++callbacks_run; }));
  }

  // Execute all pending tasks without advancing virtual time.
  task_runner_->RunUntilIdle();

  // We should only have advanced through the burst mode tasks.
  ASSERT_EQ(callbacks_run, kDesiredQPS);

  // Move time forward, should cause a token to be refreshed and an operation to
  // run.
  int last_callbacks_run = callbacks_run;
  for (int i = kDesiredQPS; i < kTasksToQueue; ++i) {
    ASSERT_FALSE(operations[i]->HasTaskExecuted());

    task_runner_->FastForwardBy(kTokenRefreshRate);

    ASSERT_EQ(callbacks_run, last_callbacks_run + 1);
    ASSERT_TRUE(operations[i]->HasTaskExecuted());
    last_callbacks_run = callbacks_run;
  }

  ASSERT_EQ(kTasksToQueue, callbacks_run);
}

// Ensure that invalidated tasks are not run, but also do not block the queue.
TEST_F(DriveOperationQueueTest, InvalidateTasks) {
  constexpr int kTasksToInvalidate = kDesiredQPS * 2;
  constexpr int kAdditionalTasks = 5;
  constexpr int kTasksToQueue =
      kDesiredQPS + kAdditionalTasks + kTasksToInvalidate;

  std::vector<std::unique_ptr<FakeDriveOperation>> operations;

  for (int i = 0; i < kTasksToQueue; ++i) {
    operations.emplace_back(std::make_unique<FakeDriveOperation>());
  }

  int callbacks_run = 0;

  for (int i = 0; i < kTasksToQueue; ++i) {
    operation_queue_->AddOperation(
        operations[i]->GetWeakPtr(),
        base::BindOnce(&FakeDriveOperation::FakeOperation,
                       operations[i]->GetWeakPtr()),
        base::BindLambdaForTesting(
            [&callbacks_run](drive::FileError) { ++callbacks_run; }));
  }

  // Execute all pending tasks without advancing virtual time.
  task_runner_->RunUntilIdle();

  // We should only have advanced through the burst mode tasks.
  ASSERT_EQ(callbacks_run, kDesiredQPS);

  // Erase tasks from the queue, which will invalidate their weak ptrs and
  // prevent callbacks from being run.
  operations.erase(operations.begin(),
                   operations.begin() + callbacks_run + kTasksToInvalidate);

  // Fast forward until all tasks have been executed.
  int last_callbacks_run = callbacks_run;
  task_runner_->FastForwardUntilNoTasksRemain();

  // All expected tasks should now have been run.
  ASSERT_EQ(last_callbacks_run + kAdditionalTasks, callbacks_run);
}

// Ensure that tasks added after burst mode has finished are only executed
// at the desired QPS.
TEST_F(DriveOperationQueueTest, DoNotRepeatBurstMode) {
  constexpr int kTasksToCreate = kDesiredQPS * 2;

  std::vector<std::unique_ptr<FakeDriveOperation>> operations;

  for (int i = 0; i < kTasksToCreate; ++i) {
    operations.emplace_back(std::make_unique<FakeDriveOperation>());
  }

  int callbacks_run = 0;

  // Only run the burst mode tasks.
  for (int i = 0; i < kDesiredQPS; ++i) {
    operation_queue_->AddOperation(
        operations[i]->GetWeakPtr(),
        base::BindOnce(&FakeDriveOperation::FakeOperation,
                       operations[i]->GetWeakPtr()),
        base::BindLambdaForTesting(
            [&callbacks_run](drive::FileError error) { ++callbacks_run; }));
  }

  // Execute all pending tasks without advancing virtual time.
  task_runner_->FastForwardBy(kTokenRefreshRate);

  ASSERT_EQ(callbacks_run, kDesiredQPS);

  // Add the remaining tasks to the queue, only the first should run as we've
  // only advanced 1 refresh period.
  for (int i = kDesiredQPS; i < kTasksToCreate; ++i) {
    operation_queue_->AddOperation(
        operations[i]->GetWeakPtr(),
        base::BindOnce(&FakeDriveOperation::FakeOperation,
                       operations[i]->GetWeakPtr()),
        base::BindLambdaForTesting(
            [&callbacks_run](drive::FileError error) { ++callbacks_run; }));
  }

  int last_callbacks_run = callbacks_run;
  // Just run the tasks that are scheduled now.
  task_runner_->RunUntilIdle();
  ASSERT_EQ(last_callbacks_run + 1, callbacks_run);

  // Run the rest of the tasks.
  task_runner_->FastForwardUntilNoTasksRemain();
  ASSERT_EQ(kTasksToCreate, callbacks_run);
}

// Ensure that burst mode is re-enabled if more than 1 second has advanced since
// the last task finished.
TEST_F(DriveOperationQueueTest, RepeatBurstMode) {
  constexpr int kTasksToCreate = kDesiredQPS * 3;

  std::vector<std::unique_ptr<FakeDriveOperation>> operations;

  for (int i = 0; i < kTasksToCreate; ++i) {
    operations.emplace_back(std::make_unique<FakeDriveOperation>());
  }

  int callbacks_run = 0;

  // Only run the burst mode tasks.
  for (int i = 0; i < kDesiredQPS; ++i) {
    operation_queue_->AddOperation(
        operations[i]->GetWeakPtr(),
        base::BindOnce(&FakeDriveOperation::FakeOperation,
                       operations[i]->GetWeakPtr()),
        base::BindLambdaForTesting(
            [&callbacks_run](drive::FileError error) { ++callbacks_run; }));
  }

  // Execute all pending tasks.
  task_runner_->FastForwardUntilNoTasksRemain();
  ASSERT_EQ(kDesiredQPS, callbacks_run);

  // Move forward two seconds, so that we should reset burst mode.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));

  // Add the remaining tasks to the queue, all should run in burst mode.
  for (int i = kDesiredQPS; i < kTasksToCreate; ++i) {
    operation_queue_->AddOperation(
        operations[i]->GetWeakPtr(),
        base::BindOnce(&FakeDriveOperation::FakeOperation,
                       operations[i]->GetWeakPtr()),
        base::BindLambdaForTesting(
            [&callbacks_run](drive::FileError error) { ++callbacks_run; }));
  }

  callbacks_run = 0;

  // Execute all pending tasks without advancing virtual time.
  task_runner_->RunUntilIdle();

  // We should only have advanced through the burst mode tasks.
  ASSERT_EQ(kDesiredQPS, callbacks_run);

  // The last tasks should not have been run yet.
  for (int i = 2 * kDesiredQPS; i < kTasksToCreate; ++i) {
    ASSERT_FALSE(operations[i]->HasTaskExecuted());
  }

  // Run the remaining tasks, which should happen at desired qps
  base::TimeTicks start_ticks = task_runner_->NowTicks();
  task_runner_->FastForwardUntilNoTasksRemain();

  ASSERT_LE(base::TimeDelta::FromSeconds(1),
            task_runner_->NowTicks() - start_ticks);

  // All tasks have been run.
  for (auto& operation : operations) {
    ASSERT_TRUE(operation->HasTaskExecuted());
  }
}

}  // namespace internal
}  // namespace drive
