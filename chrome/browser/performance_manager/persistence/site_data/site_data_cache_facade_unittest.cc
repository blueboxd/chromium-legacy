// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade.h"

#include "base/auto_reset.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/performance_manager/persistence/site_data/leveldb_site_data_store.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager {

TEST(SiteDataCacheFacadeTest, IsDataCacheRecordingForTesting) {
  content::TestBrowserThreadBundle test_browser_thread_bundle;
  std::unique_ptr<SiteDataCacheFactory, base::OnTaskRunnerDeleter> factory(
      SiteDataCacheFactory::CreateForTesting(
          base::CreateSequencedTaskRunner({})));

  TestingProfile profile;
  // Uses an in-memory database.
  auto use_in_memory_db_for_testing =
      LevelDBSiteDataStore::UseInMemoryDBForTesting();

  bool cache_is_recording = false;

  SiteDataCacheFacade data_cache_facade(&profile);
  data_cache_facade.WaitUntilCacheInitializedForTesting();
  base::RunLoop run_loop;
  data_cache_facade.IsDataCacheRecordingForTesting(
      base::BindLambdaForTesting([&](bool is_recording) {
        cache_is_recording = is_recording;
        run_loop.QuitClosure().Run();
      }));
  run_loop.Run();
  EXPECT_TRUE(cache_is_recording);

  base::RunLoop run_loop2;
  SiteDataCacheFacade off_record_data_cache_facade(
      profile.GetOffTheRecordProfile());
  off_record_data_cache_facade.IsDataCacheRecordingForTesting(
      base::BindLambdaForTesting([&](bool is_recording) {
        cache_is_recording = is_recording;
        run_loop2.QuitClosure().Run();
      }));
  run_loop2.Run();

  EXPECT_FALSE(cache_is_recording);
}

}  // namespace performance_manager
