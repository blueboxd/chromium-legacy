// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/floc_blocklist_service.h"

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/federated_learning/proto/blocklist.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace federated_learning {

// The purpose of this class is to expose the loaded_blocklist_ member and to
// allow monitoring the OnBlocklistLoadResult method calls.
class MockFlocBlocklistService : public FlocBlocklistService {
 public:
  using FlocBlocklistService::FlocBlocklistService;

  void OnBlocklistLoadResult(LoadedBlocklist blocklist) override {
    FlocBlocklistService::OnBlocklistLoadResult(std::move(blocklist));

    ++load_result_count_;

    if (load_result_count_ == expected_load_result_count_)
      run_loop_.Quit();
  }

  const LoadedBlocklist& loaded_blocklist() { return loaded_blocklist_; }

  void WaitForExpectedLoadResultCount(size_t expected_load_result_count) {
    DCHECK(!run_loop_.running());
    if (expected_load_result_count_ >= expected_load_result_count)
      return;

    expected_load_result_count_ = expected_load_result_count;
    run_loop_.Run();
  }

 private:
  size_t load_result_count_ = 0;
  size_t expected_load_result_count_ = 0;
  base::RunLoop run_loop_;
};

class FlocBlocklistServiceTest : public ::testing::Test {
 public:
  FlocBlocklistServiceTest()
      : background_task_runner_(
            base::MakeRefCounted<base::TestSimpleTaskRunner>()) {}

  FlocBlocklistServiceTest(const FlocBlocklistServiceTest&) = delete;
  FlocBlocklistServiceTest& operator=(const FlocBlocklistServiceTest&) = delete;

 protected:
  void SetUp() override {
    service_ = std::make_unique<MockFlocBlocklistService>();
    service_->SetBackgroundTaskRunnerForTesting(background_task_runner_);
  }

  base::FilePath GetUniqueTemporaryPath() {
    CHECK(scoped_temp_dir_.IsValid() || scoped_temp_dir_.CreateUniqueTempDir());
    return scoped_temp_dir_.GetPath().AppendASCII(
        base::NumberToString(next_unique_file_suffix_++));
  }

  base::FilePath CreateTestBlocklistProtoFile(
      const std::vector<uint64_t>& blocklist) {
    base::FilePath file_path = GetUniqueTemporaryPath();

    federated_learning::proto::Blocklist blocklist_proto;
    for (uint64_t n : blocklist)
      blocklist_proto.add_entries(n);

    std::string contents;
    CHECK(blocklist_proto.SerializeToString(&contents));

    CHECK_EQ(static_cast<int>(contents.size()),
             base::WriteFile(file_path, contents.data(),
                             static_cast<int>(contents.size())));
    return file_path;
  }

  base::FilePath CreateCorruptedTestBlocklistProtoFile() {
    base::FilePath file_path = GetUniqueTemporaryPath();
    std::string contents = "1234\n5678\n";
    CHECK_EQ(static_cast<int>(contents.size()),
             base::WriteFile(file_path, contents.data(),
                             static_cast<int>(contents.size())));
    return file_path;
  }

  MockFlocBlocklistService* service() { return service_.get(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir scoped_temp_dir_;
  int next_unique_file_suffix_ = 1;

  scoped_refptr<base::TestSimpleTaskRunner> background_task_runner_;

  std::unique_ptr<MockFlocBlocklistService> service_;
};

TEST_F(FlocBlocklistServiceTest, Startup_NoBlocklistNotNotified) {
  EXPECT_FALSE(service()->loaded_blocklist().has_value());
}

TEST_F(FlocBlocklistServiceTest, NewEmptyBlocklist_Loaded) {
  base::FilePath file_path = CreateTestBlocklistProtoFile({});
  service()->OnBlocklistFileReady(file_path);

  background_task_runner_->RunPendingTasks();
  service()->WaitForExpectedLoadResultCount(1u);

  EXPECT_TRUE(service()->loaded_blocklist().has_value());
  EXPECT_EQ(service()->loaded_blocklist()->size(), 0u);
}

TEST_F(FlocBlocklistServiceTest, NewNonEmptyBlocklist_Loaded) {
  base::FilePath file_path = CreateTestBlocklistProtoFile({1, 2, 3, 0});
  service()->OnBlocklistFileReady(file_path);

  background_task_runner_->RunPendingTasks();
  service()->WaitForExpectedLoadResultCount(1u);

  EXPECT_TRUE(service()->loaded_blocklist().has_value());
  EXPECT_EQ(service()->loaded_blocklist()->size(), 4u);
  EXPECT_TRUE(service()->loaded_blocklist()->count(0));
  EXPECT_TRUE(service()->loaded_blocklist()->count(1));
  EXPECT_TRUE(service()->loaded_blocklist()->count(2));
  EXPECT_TRUE(service()->loaded_blocklist()->count(3));
}

TEST_F(FlocBlocklistServiceTest, NonExistentBlocklist_NotLoaded) {
  base::FilePath file_path = GetUniqueTemporaryPath();
  service()->OnBlocklistFileReady(file_path);

  background_task_runner_->RunPendingTasks();
  service()->WaitForExpectedLoadResultCount(1u);

  EXPECT_FALSE(service()->loaded_blocklist().has_value());
}

TEST_F(FlocBlocklistServiceTest, CorruptedBlocklist_NotLoaded) {
  base::FilePath file_path = CreateCorruptedTestBlocklistProtoFile();
  service()->OnBlocklistFileReady(file_path);

  background_task_runner_->RunPendingTasks();
  service()->WaitForExpectedLoadResultCount(1u);

  EXPECT_FALSE(service()->loaded_blocklist().has_value());
}

TEST_F(FlocBlocklistServiceTest, MultipleUpdate_LatestOneLoaded) {
  base::FilePath file_path1 = CreateTestBlocklistProtoFile({1, 2, 3, 0});
  base::FilePath file_path2 = CreateTestBlocklistProtoFile({4});
  service()->OnBlocklistFileReady(file_path1);
  service()->OnBlocklistFileReady(file_path2);

  EXPECT_FALSE(service()->loaded_blocklist().has_value());

  background_task_runner_->RunPendingTasks();
  service()->WaitForExpectedLoadResultCount(2u);

  EXPECT_TRUE(service()->loaded_blocklist().has_value());
  EXPECT_EQ(service()->loaded_blocklist()->size(), 1u);
  EXPECT_TRUE(service()->loaded_blocklist()->count(4));
}

}  // namespace federated_learning