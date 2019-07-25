// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_inspector.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/services/util_win/util_win_impl.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::FilePath GetKernel32DllFilePath() {
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::string sysroot;
  EXPECT_TRUE(env->GetVar("SYSTEMROOT", &sysroot));

  base::FilePath path =
      base::FilePath::FromUTF8Unsafe(sysroot).Append(L"system32\\kernel32.dll");

  return path;
}

bool CreateInspectionResultsCacheWithEntry(
    const ModuleInfoKey& module_key,
    const ModuleInspectionResult& inspection_result) {
  // First create a cache with bogus data and create the cache file.
  InspectionResultsCache inspection_results_cache;

  AddInspectionResultToCache(module_key, inspection_result,
                             &inspection_results_cache);

  return WriteInspectionResultsCache(
      ModuleInspector::GetInspectionResultsCachePath(),
      inspection_results_cache);
}

class ModuleInspectorTest : public testing::Test {
 public:
  ModuleInspectorTest()
      : test_browser_thread_bundle_(
            base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME) {}

  // Callback for ModuleInspector.
  void OnModuleInspected(const ModuleInfoKey& module_key,
                         ModuleInspectionResult inspection_result) {
    inspected_modules_.push_back(std::move(inspection_result));
  }

  void RunUntilIdle() { test_browser_thread_bundle_.RunUntilIdle(); }
  void FastForwardToIdleTimer() {
    test_browser_thread_bundle_.FastForwardBy(
        ModuleInspector::kFlushInspectionResultsTimerTimeout);
    test_browser_thread_bundle_.RunUntilIdle();
  }

  const std::vector<ModuleInspectionResult>& inspected_modules() {
    return inspected_modules_;
  }

  void ClearInspectedModules() { inspected_modules_.clear(); }

  // A TestBrowserThreadBundle is required instead of a ScopedTaskEnvironment
  // because of AfterStartupTaskUtils (DCHECK for BrowserThread::UI).
  //
  // Must be before the ModuleInspector.
  content::TestBrowserThreadBundle test_browser_thread_bundle_;

 private:
  std::vector<ModuleInspectionResult> inspected_modules_;

  DISALLOW_COPY_AND_ASSIGN(ModuleInspectorTest);
};

}  // namespace

TEST_F(ModuleInspectorTest, OneModule) {
  ModuleInspector module_inspector(base::BindRepeating(
      &ModuleInspectorTest::OnModuleInspected, base::Unretained(this)));

  module_inspector.AddModule({GetKernel32DllFilePath(), 0, 0});

  RunUntilIdle();

  ASSERT_EQ(1u, inspected_modules().size());
}

TEST_F(ModuleInspectorTest, MultipleModules) {
  ModuleInfoKey kTestCases[] = {
      {base::FilePath(), 0, 0}, {base::FilePath(), 0, 0},
      {base::FilePath(), 0, 0}, {base::FilePath(), 0, 0},
      {base::FilePath(), 0, 0},
  };

  ModuleInspector module_inspector(base::BindRepeating(
      &ModuleInspectorTest::OnModuleInspected, base::Unretained(this)));

  for (const auto& module : kTestCases)
    module_inspector.AddModule(module);

  RunUntilIdle();

  EXPECT_EQ(5u, inspected_modules().size());
}

TEST_F(ModuleInspectorTest, DisableBackgroundInspection) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      ModuleInspector::kDisableBackgroundModuleInspection);

  ModuleInfoKey kTestCases[] = {
      {base::FilePath(), 0, 0},
      {base::FilePath(), 0, 0},
  };

  ModuleInspector module_inspector(base::BindRepeating(
      &ModuleInspectorTest::OnModuleInspected, base::Unretained(this)));

  for (const auto& module : kTestCases)
    module_inspector.AddModule(module);

  RunUntilIdle();

  // No inspected modules yet.
  EXPECT_EQ(0u, inspected_modules().size());

  // Increasing inspection priority will start the background inspection
  // process.
  module_inspector.IncreaseInspectionPriority();
  RunUntilIdle();

  EXPECT_EQ(2u, inspected_modules().size());
}

TEST_F(ModuleInspectorTest, OOPInspectModule) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      ModuleInspector::kWinOOPInspectModuleFeature);

  ModuleInfoKey kTestCases[] = {
      {base::FilePath(), 0, 0},
      {base::FilePath(), 0, 0},
  };

  ModuleInspector module_inspector(base::BindRepeating(
      &ModuleInspectorTest::OnModuleInspected, base::Unretained(this)));

  mojo::PendingRemote<chrome::mojom::UtilWin> remote;
  UtilWinImpl util_win_impl(remote.InitWithNewPipeAndPassReceiver());
  module_inspector.SetRemoteUtilWinForTesting(std::move(remote));

  for (const auto& module : kTestCases)
    module_inspector.AddModule(module);

  RunUntilIdle();
  EXPECT_EQ(2u, inspected_modules().size());
}

TEST_F(ModuleInspectorTest, InspectionResultsCache) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kInspectionResultsCache);

  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::ScopedPathOverride scoped_user_data_dir_override(
      chrome::DIR_USER_DATA, scoped_temp_dir.GetPath());

  // First create a cache with bogus data and create the cache file.
  ModuleInfoKey module_key(GetKernel32DllFilePath(), 0, 0);
  ModuleInspectionResult inspection_result;
  inspection_result.location = L"BogusLocation";
  inspection_result.basename = L"BogusBasename";

  ASSERT_TRUE(
      CreateInspectionResultsCacheWithEntry(module_key, inspection_result));

  ModuleInspector module_inspector(base::BindRepeating(
      &ModuleInspectorTest::OnModuleInspected, base::Unretained(this)));

  module_inspector.AddModule(module_key);

  RunUntilIdle();

  ASSERT_EQ(1u, inspected_modules().size());

  // The following comparisons can only succeed if the module was truly read
  // from the cache.
  ASSERT_EQ(inspected_modules()[0].location, inspection_result.location);
  ASSERT_EQ(inspected_modules()[0].basename, inspection_result.basename);
}

// Tests that when OnModuleDatabaseIdle() notificate is received, the cache is
// flushed to disk.
TEST_F(ModuleInspectorTest, InspectionResultsCache_OnModuleDatabaseIdle) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kInspectionResultsCache);

  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::ScopedPathOverride scoped_user_data_dir_override(
      chrome::DIR_USER_DATA, scoped_temp_dir.GetPath());

  ModuleInspector module_inspector(base::BindRepeating(
      &ModuleInspectorTest::OnModuleInspected, base::Unretained(this)));

  ModuleInfoKey module_key(GetKernel32DllFilePath(), 0, 0);
  module_inspector.AddModule(module_key);

  RunUntilIdle();

  ASSERT_EQ(1u, inspected_modules().size());

  module_inspector.OnModuleDatabaseIdle();
  RunUntilIdle();

  // If the cache was written to disk, it should contain the one entry for
  // Kernel32.dll.
  InspectionResultsCache inspection_results_cache;
  EXPECT_EQ(ReadInspectionResultsCache(
                ModuleInspector::GetInspectionResultsCachePath(), 0,
                &inspection_results_cache),
            ReadCacheResult::kSuccess);

  EXPECT_EQ(inspection_results_cache.size(), 1u);
  auto inspection_result =
      GetInspectionResultFromCache(module_key, &inspection_results_cache);
  EXPECT_TRUE(inspection_result);
}

// Tests that when the timer expires before the OnModuleDatabaseIdle()
// notification, the cache is flushed to disk.
TEST_F(ModuleInspectorTest, InspectionResultsCache_TimerExpired) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kInspectionResultsCache);

  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::ScopedPathOverride scoped_user_data_dir_override(
      chrome::DIR_USER_DATA, scoped_temp_dir.GetPath());

  ModuleInspector module_inspector(base::BindRepeating(
      &ModuleInspectorTest::OnModuleInspected, base::Unretained(this)));

  ModuleInfoKey module_key(GetKernel32DllFilePath(), 0, 0);
  module_inspector.AddModule(module_key);

  RunUntilIdle();

  ASSERT_EQ(1u, inspected_modules().size());

  // Fast forwarding until the timer is fired.
  FastForwardToIdleTimer();

  // If the cache was flushed, it should contain the one entry for Kernel32.dll.
  InspectionResultsCache inspection_results_cache;
  EXPECT_EQ(ReadInspectionResultsCache(
                ModuleInspector::GetInspectionResultsCachePath(), 0,
                &inspection_results_cache),
            ReadCacheResult::kSuccess);

  EXPECT_EQ(inspection_results_cache.size(), 1u);
  auto inspection_result =
      GetInspectionResultFromCache(module_key, &inspection_results_cache);
  EXPECT_TRUE(inspection_result);
}
