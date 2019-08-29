// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/ml_app_rank_provider.h"

#include "base/test/task_environment.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_event_logger.pb.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

const char kAppId[] = "app_id";

TEST(MlAppRankProviderTest, MlInferenceTest) {
  base::test::TaskEnvironment task_environment_;

  chromeos::machine_learning::FakeServiceConnectionImpl fake_service_connection;

  const double expected_value = 1.234;
  fake_service_connection.SetOutputValue(std::vector<int64_t>{1L},
                                         std::vector<double>{expected_value});

  chromeos::machine_learning::ServiceConnection::
      UseFakeServiceConnectionForTesting(&fake_service_connection);

  MlAppRankProvider ml_app_rank_provider;

  base::flat_map<std::string, AppLaunchFeatures> app_features_map;
  AppLaunchFeatures features;
  features.set_app_id(kAppId);
  features.set_app_type(AppLaunchEvent_AppType_CHROME);
  features.set_click_rank(1);
  for (int hour = 0; hour < 24; ++hour) {
    features.add_clicks_each_hour(1);
  }

  app_features_map[kAppId] = features;
  ml_app_rank_provider.CreateRankings(app_features_map, 3, 1, 7);

  EXPECT_EQ(0UL, ml_app_rank_provider.RetrieveRankings().size());

  task_environment_.RunUntilIdle();

  const std::map<std::string, float> ranking_map =
      ml_app_rank_provider.RetrieveRankings();

  task_environment_.RunUntilIdle();

  ASSERT_EQ(1UL, ranking_map.size());
  const auto it = ranking_map.find(kAppId);
  ASSERT_NE(ranking_map.end(), it);
  EXPECT_NEAR(expected_value, it->second, 0.001);
}

TEST(MlAppRankProviderTest, ExecutionAfterDestructorTest) {
  base::test::TaskEnvironment task_environment_;

  chromeos::machine_learning::FakeServiceConnectionImpl fake_service_connection;

  const double expected_value = 1.234;
  fake_service_connection.SetOutputValue(std::vector<int64_t>{1L},
                                         std::vector<double>{expected_value});

  chromeos::machine_learning::ServiceConnection::
      UseFakeServiceConnectionForTesting(&fake_service_connection);

  {
    MlAppRankProvider ml_app_rank_provider;

    base::flat_map<std::string, AppLaunchFeatures> app_features_map;
    AppLaunchFeatures features;
    features.set_app_id(kAppId);
    features.set_app_type(AppLaunchEvent_AppType_CHROME);
    app_features_map[kAppId] = features;
    ml_app_rank_provider.CreateRankings(app_features_map, 3, 1, 7);
  }
  // Run the background tasks after ml_app_rank_provider has been destroyed.
  // If this does not crash it is a success.
  task_environment_.RunUntilIdle();
}

}  // namespace app_list
