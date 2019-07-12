// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_factory.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

class SiteDataCacheFactoryTest : public ::testing::Test {
 protected:
  SiteDataCacheFactoryTest()
      : task_runner_(base::CreateSequencedTaskRunner({})),
        factory_(SiteDataCacheFactory::CreateForTesting(task_runner_)) {}

  ~SiteDataCacheFactoryTest() override {
    factory_.reset();
    test_browser_thread_bundle_.RunUntilIdle();
  }

  void SetUp() override {
    EXPECT_EQ(SiteDataCacheFactory::GetInstance(), factory_.get());
  }

  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<SiteDataCacheFactory, base::OnTaskRunnerDeleter> factory_;
  TestingProfile profile_;

  DISALLOW_COPY_AND_ASSIGN(SiteDataCacheFactoryTest);
};

TEST_F(SiteDataCacheFactoryTest, EndToEnd) {
  SiteDataCacheFactory::OnBrowserContextCreatedOnUIThread(factory_.get(),
                                                          &profile_, nullptr);

  base::RunLoop run_loop;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](SiteDataCacheFactory* factory,
             const std::string& browser_context_id,
             base::OnceClosure quit_closure) {
            DCHECK_NE(nullptr, factory->GetDataCacheForBrowserContext(
                                   browser_context_id));
            DCHECK_NE(nullptr, factory->GetInspectorForBrowserContext(
                                   browser_context_id));
            std::move(quit_closure).Run();
          },
          factory_.get(), profile_.UniqueId(), run_loop.QuitClosure()));
  run_loop.Run();

  SiteDataCacheFactory::OnBrowserContextDestroyedOnUIThread(factory_.get(),
                                                            &profile_);

  base::RunLoop run_loop2;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](SiteDataCacheFactory* factory,
             const std::string& browser_context_id,
             base::OnceClosure quit_closure) {
            DCHECK_EQ(nullptr, factory->GetDataCacheForBrowserContext(
                                   browser_context_id));
            DCHECK_EQ(nullptr, factory->GetInspectorForBrowserContext(
                                   browser_context_id));
            std::move(quit_closure).Run();
          },
          factory_.get(), profile_.UniqueId(), run_loop.QuitClosure()));
  test_browser_thread_bundle_.RunUntilIdle();
}

}  // namespace performance_manager
