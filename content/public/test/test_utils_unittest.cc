// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_utils.h"

#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

// Regression test for crbug.com/1035189.
TEST(ContentTestUtils, NestedRunAllTasksUntilIdleWithPendingThreadPoolWork) {
  base::test::TaskEnvironment task_environment;

  bool thread_pool_task_completed = false;
  base::PostTask(
      FROM_HERE, {base::ThreadPool()}, base::BindLambdaForTesting([&]() {
        base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(200));
        thread_pool_task_completed = true;
      }));

  base::RunLoop run_loop;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Nested RunAllTasksUntilIdle() (i.e. crbug.com/1035189).
        content::RunAllTasksUntilIdle();
        EXPECT_TRUE(thread_pool_task_completed);
        run_loop.Quit();
      }));

  run_loop.Run();
  EXPECT_TRUE(thread_pool_task_completed);
}
