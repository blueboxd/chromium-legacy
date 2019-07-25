// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/demographic_metrics_provider.h"

#include <memory>

#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/user_demographics.h"
#include "components/sync/driver/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics {
namespace {

// Profile client for testing that gets fake Profile information and services.
class TestProfileClient : public DemographicMetricsProvider::ProfileClient {
 public:
  ~TestProfileClient() override = default;

  TestProfileClient(std::unique_ptr<syncer::SyncService> sync_service,
                    int number_of_profiles)
      : sync_service_(std::move(sync_service)),
        number_of_profiles_(number_of_profiles) {}

  int GetNumberOfProfilesOnDisk() override { return number_of_profiles_; }

  syncer::SyncService* GetSyncService() override { return sync_service_.get(); }

  base::Time GetNetworkTime() const override {
    base::Time time;
    auto result = base::Time::FromString("17 Jun 2019 00:00:00 UDT", &time);
    DCHECK(result);
    return time;
  }

 private:
  std::unique_ptr<syncer::SyncService> sync_service_;
  const int number_of_profiles_;
  base::SimpleTestClock clock_;

  DISALLOW_COPY_AND_ASSIGN(TestProfileClient);
};

// Make arbitrary user demographics to provide.
base::Optional<syncer::UserDemographics> GetDemographics() {
  base::Optional<syncer::UserDemographics> user_demographics =
      syncer::UserDemographics();
  user_demographics->birth_year = 1983;
  user_demographics->gender = UserDemographicsProto::GENDER_FEMALE;
  return user_demographics;
}

std::unique_ptr<TestProfileClient> MakeTestProfileClient(
    const base::Optional<syncer::UserDemographics>& user_demographics,
    int number_of_profiles,
    bool has_sync_service) {
  std::unique_ptr<syncer::TestSyncService> sync_service = nullptr;
  if (has_sync_service) {
    sync_service = std::make_unique<syncer::TestSyncService>();
    sync_service->SetUserDemographics(user_demographics);
  }
  return std::make_unique<TestProfileClient>(std::move(sync_service),
                                             number_of_profiles);
}

TEST(DemographicMetricsProviderTest, ProvideCurrentSessionData_FeatureEnabled) {
  // Enable demographics reporting feature.
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndEnableFeature(
      DemographicMetricsProvider::kDemographicMetricsReporting);

  // Run demographics provider.
  DemographicMetricsProvider provider(
      MakeTestProfileClient(GetDemographics(), /*number_of_profiles=*/1,
                            /*has_sync_service=*/true));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);

  // Verify provided demographics.
  EXPECT_EQ(GetDemographics()->birth_year,
            uma_proto.user_demographics().birth_year());
  EXPECT_EQ(GetDemographics()->gender, uma_proto.user_demographics().gender());
}

TEST(DemographicMetricsProviderTest, ProvideCurrentSessionData_NoSyncService) {
  // Enable demographics reporting feature.
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndDisableFeature(
      DemographicMetricsProvider::kDemographicMetricsReporting);

  // Run demographics provider.
  DemographicMetricsProvider provider(
      MakeTestProfileClient(GetDemographics(),
                            /*number_of_profiles=*/1,
                            /*has_sync_service=*/false));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);

  // Expect the proto fields to be not set and left to default.
  EXPECT_FALSE(uma_proto.user_demographics().has_birth_year());
  EXPECT_FALSE(uma_proto.user_demographics().has_gender());
}

TEST(DemographicMetricsProviderTest,
     ProvideCurrentSessionData_FeatureDisabled) {
  // Disable demographics reporting feature.
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndDisableFeature(
      DemographicMetricsProvider::kDemographicMetricsReporting);

  // Run demographics provider.
  DemographicMetricsProvider provider(
      MakeTestProfileClient(GetDemographics(), /*number_of_profiles=*/1,
                            /*has_sync_service=*/true));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);

  // Expect that the UMA proto is untouched.
  EXPECT_FALSE(uma_proto.user_demographics().has_birth_year());
  EXPECT_FALSE(uma_proto.user_demographics().has_gender());
}

TEST(DemographicMetricsProviderTest,
     ProvideCurrentSessionData_NotExactlyOneProfile) {
  // Enable demographics reporting feature.
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndEnableFeature(
      DemographicMetricsProvider::kDemographicMetricsReporting);

  // Run demographics provider with not exactly one Profile on disk.
  DemographicMetricsProvider provider(
      MakeTestProfileClient(GetDemographics(), /*number_of_profiles=*/2,
                            /*has_sync_service=*/true));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);

  // Expect that the UMA proto is untouched.
  EXPECT_FALSE(uma_proto.user_demographics().has_birth_year());
  EXPECT_FALSE(uma_proto.user_demographics().has_gender());
}

TEST(DemographicMetricsProviderTest,
     ProvideCurrentSessionData_NoUserDemographics) {
  // Enable demographics reporting feature.
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndEnableFeature(
      DemographicMetricsProvider::kDemographicMetricsReporting);

  // Run demographics provider with a ProfileClient that does not provide
  // demographics.
  DemographicMetricsProvider provider(
      MakeTestProfileClient(base::nullopt, /*number_of_profiles=*/1,
                            /*has_sync_service=*/true));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);

  // Expect that the UMA proto is untouched.
  EXPECT_FALSE(uma_proto.user_demographics().has_birth_year());
  EXPECT_FALSE(uma_proto.user_demographics().has_gender());
}

}  // namespace
}  // namespace metrics