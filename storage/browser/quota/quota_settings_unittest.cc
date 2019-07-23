// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "storage/browser/quota/quota_disk_info_helper.h"
#include "storage/browser/quota/quota_features.h"
#include "storage/browser/quota/quota_settings.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

namespace storage {

class MockQuotaDiskInfoHelper : public QuotaDiskInfoHelper {
 public:
  MockQuotaDiskInfoHelper() = default;
  MOCK_CONST_METHOD1(AmountOfTotalDiskSpace, int64_t(const base::FilePath&));
};

class QuotaSettingsTest : public testing::Test {
 public:
  QuotaSettingsTest() = default;
  void SetUp() override { ASSERT_TRUE(data_dir_.CreateUniqueTempDir()); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  const base::FilePath& profile_path() const { return data_dir_.GetPath(); }

 private:
  base::ScopedTempDir data_dir_;
  QuotaSettings quota_settings_;
  DISALLOW_COPY_AND_ASSIGN(QuotaSettingsTest);
};

TEST_F(QuotaSettingsTest, ExpandedTempPool) {
  MockQuotaDiskInfoHelper disk_info_helper;
  ON_CALL(disk_info_helper, AmountOfTotalDiskSpace(_))
      .WillByDefault(::testing::Return(2000));
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kQuotaExpandPoolSize,
      {{"PoolSizeRatio", "0.75"}, {"PerHostRatio", "0.5"}});

  bool callback_executed = false;
  GetNominalDynamicSettings(
      profile_path(), false, &disk_info_helper,
      base::BindLambdaForTesting([&](base::Optional<QuotaSettings> settings) {
        callback_executed = true;
        ASSERT_NE(settings, base::nullopt);
        // 1500 = 2000 * PoolSizeRatio
        EXPECT_EQ(settings->pool_size, 1500);
        // 750 = 1500 * PerHostRatio
        EXPECT_EQ(settings->per_host_quota, 750);
      }));
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_executed);
}

TEST_F(QuotaSettingsTest, UnlimitedTempPool) {
  MockQuotaDiskInfoHelper disk_info_helper;
  ON_CALL(disk_info_helper, AmountOfTotalDiskSpace(_))
      .WillByDefault(::testing::Return(2000));
  scoped_feature_list_.InitAndEnableFeature(features::kQuotaUnlimitedPoolSize);

  bool callback_executed = false;
  GetNominalDynamicSettings(
      profile_path(), false, &disk_info_helper,
      base::BindLambdaForTesting([&](base::Optional<QuotaSettings> settings) {
        callback_executed = true;
        ASSERT_NE(settings, base::nullopt);
        EXPECT_EQ(settings->pool_size, 2000);
        EXPECT_EQ(settings->per_host_quota, 2000);
      }));
  scoped_task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_executed);
}
}  // namespace storage
