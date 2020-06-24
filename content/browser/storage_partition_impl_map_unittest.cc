// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_partition_impl_map.h"

#include <unordered_set>
#include <utility>

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(StoragePartitionImplMapTest, GarbageCollect) {
  BrowserTaskEnvironment task_environment;
  TestBrowserContext browser_context;
  StoragePartitionImplMap storage_partition_impl_map(&browser_context);

  std::unique_ptr<std::unordered_set<base::FilePath>> active_paths(
      new std::unordered_set<base::FilePath>);

  base::FilePath active_path = browser_context.GetPath().Append(
      StoragePartitionImplMap::GetStoragePartitionPath(
          "active", std::string()));
  ASSERT_TRUE(base::CreateDirectory(active_path));
  active_paths->insert(active_path);

  base::FilePath inactive_path = browser_context.GetPath().Append(
      StoragePartitionImplMap::GetStoragePartitionPath(
          "inactive", std::string()));
  ASSERT_TRUE(base::CreateDirectory(inactive_path));

  base::RunLoop run_loop;
  storage_partition_impl_map.GarbageCollect(std::move(active_paths),
                                            run_loop.QuitClosure());
  run_loop.Run();
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);

  EXPECT_TRUE(base::PathExists(active_path));
  EXPECT_FALSE(base::PathExists(inactive_path));
}

}  // namespace content
