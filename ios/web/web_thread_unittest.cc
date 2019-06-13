// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/thread/web_thread.h"
#include "base/bind.h"
#include "base/task/post_task.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/web_thread_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {

class WebThreadTest : public PlatformTest {
 protected:
  static void BasicFunction(base::OnceClosure continuation,
                            WebThread::ID target) {
    EXPECT_TRUE(WebThread::CurrentlyOn(target));
    std::move(continuation).Run();
  }

  web::TestWebThreadBundle web_thread_bundle_;
};

TEST_F(WebThreadTest, PostTaskWithTraits) {
  base::RunLoop run_loop;
  EXPECT_TRUE(base::PostTaskWithTraits(
      FROM_HERE, {WebThread::IO},
      base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                     WebThread::IO)));
  run_loop.Run();
}

TEST_F(WebThreadTest, PostTaskViaTaskRunnerWithTraits) {
  scoped_refptr<base::TaskRunner> task_runner =
      base::CreateTaskRunnerWithTraits({WebThread::IO});
  base::RunLoop run_loop;
  EXPECT_TRUE(task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                WebThread::IO)));
  run_loop.Run();
}

TEST_F(WebThreadTest, PostTaskViaSequencedTaskRunnerWithTraits) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::CreateSequencedTaskRunnerWithTraits({WebThread::IO});
  base::RunLoop run_loop;
  EXPECT_TRUE(task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                WebThread::IO)));
  run_loop.Run();
}

TEST_F(WebThreadTest, PostTaskViaSingleThreadTaskRunnerWithTraits) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::CreateSingleThreadTaskRunnerWithTraits({WebThread::IO});
  base::RunLoop run_loop;
  EXPECT_TRUE(task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                WebThread::IO)));
  run_loop.Run();
}

}  // namespace web
