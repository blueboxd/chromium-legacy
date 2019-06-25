// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"

#include <exception>
#include <memory>
#include <vector>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash/hash.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor_test_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_config.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::FloatEq;
using testing::Pair;
using testing::UnorderedElementsAre;

namespace app_list {

RecurrencePredictorProto MakeTestingProto() {
  RecurrencePredictorProto proto;
  auto* hour_bin_proto = proto.mutable_hour_bin_predictor();
  hour_bin_proto->set_last_decay_timestamp(365);

  HourBinPredictorProto::FrequencyTable frequency_table;
  (*frequency_table.mutable_frequency())[1u] = 3;
  (*frequency_table.mutable_frequency())[2u] = 1;
  frequency_table.set_total_counts(4);
  (*hour_bin_proto->mutable_binned_frequency_table())[10] = frequency_table;

  frequency_table = HourBinPredictorProto::FrequencyTable();
  (*frequency_table.mutable_frequency())[1u] = 1;
  (*frequency_table.mutable_frequency())[3u] = 1;
  frequency_table.set_total_counts(2);
  (*hour_bin_proto->mutable_binned_frequency_table())[11] = frequency_table;

  return proto;
}

class FrecencyPredictorTest : public testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();

    config_.set_decay_coeff(0.5f);
    predictor_ = std::make_unique<FrecencyPredictor>(config_);
  }

  FrecencyPredictorConfig config_;
  std::unique_ptr<FrecencyPredictor> predictor_;
};

class HourBinPredictorTest : public testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();

    config_.set_weekly_decay_coeff(0.5f);
    (*config_.mutable_bin_weights_map())[0] = 0.6;
    (*config_.mutable_bin_weights_map())[1] = 0.15;
    (*config_.mutable_bin_weights_map())[2] = 0.05;
    (*config_.mutable_bin_weights_map())[-1] = 0.15;
    (*config_.mutable_bin_weights_map())[-2] = 0.05;
    predictor_ = std::make_unique<HourBinPredictor>(config_);
  }

  // Sets local time according to |day_of_week| and |hour_of_day|.
  void SetLocalTime(const int day_of_week, const int hour_of_day) {
    AdvanceToNextLocalSunday();
    const auto advance = base::TimeDelta::FromDays(day_of_week) +
                         base::TimeDelta::FromHours(hour_of_day);
    if (advance > base::TimeDelta()) {
      time_.Advance(advance);
    }
  }

  base::ScopedMockClockOverride time_;
  HourBinPredictorConfig config_;
  std::unique_ptr<HourBinPredictor> predictor_;

 private:
  // Advances time to be 0am next Sunday.
  void AdvanceToNextLocalSunday() {
    base::Time::Exploded now;
    base::Time::Now().LocalExplode(&now);
    const auto advance = base::TimeDelta::FromDays(6 - now.day_of_week) +
                         base::TimeDelta::FromHours(24 - now.hour);
    if (advance > base::TimeDelta()) {
      time_.Advance(advance);
    }
    base::Time::Now().LocalExplode(&now);
    CHECK_EQ(now.day_of_week, 0);
    CHECK_EQ(now.hour, 0);
  }
};

TEST_F(FrecencyPredictorTest, RankWithNoTargets) {
  EXPECT_TRUE(predictor_->Rank(0u).empty());
}

TEST_F(FrecencyPredictorTest, RecordAndRankSimple) {
  predictor_->Train(2u, 0u);
  predictor_->Train(4u, 0u);
  predictor_->Train(6u, 0u);

  EXPECT_THAT(
      predictor_->Rank(0u),
      UnorderedElementsAre(Pair(2u, FloatEq(0.125f)), Pair(4u, FloatEq(0.25f)),
                           Pair(6u, FloatEq(0.5f))));
}

TEST_F(FrecencyPredictorTest, RecordAndRankComplex) {
  predictor_->Train(2u, 0u);
  predictor_->Train(4u, 0u);
  predictor_->Train(6u, 0u);
  predictor_->Train(4u, 0u);
  predictor_->Train(2u, 0u);

  // Ranks should be deterministic.
  for (int i = 0; i < 3; ++i) {
    EXPECT_THAT(predictor_->Rank(0u),
                UnorderedElementsAre(Pair(2u, FloatEq(0.53125f)),
                                     Pair(4u, FloatEq(0.3125f)),
                                     Pair(6u, FloatEq(0.125f))));
  }
}

TEST_F(FrecencyPredictorTest, ToAndFromProto) {
  predictor_->Train(1u, 0u);
  predictor_->Train(3u, 0u);
  predictor_->Train(5u, 0u);

  RecurrencePredictorProto proto;
  predictor_->ToProto(&proto);

  FrecencyPredictor new_predictor(config_);
  new_predictor.FromProto(proto);

  EXPECT_TRUE(proto.has_frecency_predictor());
  EXPECT_EQ(proto.frecency_predictor().num_updates(), 3u);
  EXPECT_EQ(predictor_->Rank(0u), new_predictor.Rank(0u));
}

TEST_F(HourBinPredictorTest, RankWithNoTargets) {
  EXPECT_TRUE(predictor_->Rank(0u).empty());
}

TEST_F(HourBinPredictorTest, GetTheRightBin) {
  // Monday.
  for (int i = 0; i <= 23; ++i) {
    SetLocalTime(1, i);
    EXPECT_EQ(predictor_->GetBin(), i);
  }

  // Friday.
  for (int i = 0; i <= 23; ++i) {
    SetLocalTime(5, i);
    EXPECT_EQ(predictor_->GetBin(), i);
  }

  // Saturday.
  for (int i = 0; i <= 23; ++i) {
    SetLocalTime(6, i);
    EXPECT_EQ(predictor_->GetBin(), i + 24);
  }

  // Sunday.
  for (int i = 0; i <= 23; ++i) {
    SetLocalTime(0, i);
    EXPECT_EQ(predictor_->GetBin(), i + 24);
  }

  // 2 hour before 00:00 Monday is 22:00 Sunday
  SetLocalTime(1, 0);
  EXPECT_EQ(predictor_->GetBinFromHourDifference(-2), 22 + 24);

  // 3 hour after 23:00 Friday is 02:00 Saturday
  SetLocalTime(5, 23);
  EXPECT_EQ(predictor_->GetBinFromHourDifference(3), 2 + 24);

  // 4 hour after 22:00 Sunday is 2:00 Monday
  SetLocalTime(0, 22);
  EXPECT_EQ(predictor_->GetBinFromHourDifference(4), 2);

  // 5 hour before 3:00 Saturday is 22:00 Friday
  SetLocalTime(6, 3);
  EXPECT_EQ(predictor_->GetBinFromHourDifference(-5), 22);
}

TEST_F(HourBinPredictorTest, TrainAndRankSingleBin) {
  base::flat_map<int, float> weights(
      predictor_->config_.bin_weights_map().begin(),
      predictor_->config_.bin_weights_map().end());

  SetLocalTime(1, 10);
  predictor_->Train(1u, 0u);
  SetLocalTime(2, 10);
  predictor_->Train(1u, 0u);
  SetLocalTime(3, 10);
  predictor_->Train(2u, 0u);
  SetLocalTime(4, 10);
  predictor_->Train(1u, 0u);
  SetLocalTime(5, 10);
  predictor_->Train(2u, 0u);

  // Train on weekend doesn't affect the result during the week
  SetLocalTime(0, 10);
  predictor_->Train(1u, 0u);
  SetLocalTime(0, 10);
  predictor_->Train(2u, 0u);

  SetLocalTime(1, 10);
  EXPECT_THAT(predictor_->Rank(0u),
              UnorderedElementsAre(Pair(1u, FloatEq(weights[0] * 0.6)),
                                   Pair(2u, FloatEq((weights)[0] * 0.4))));
}

TEST_F(HourBinPredictorTest, TrainAndRankMultipleBin) {
  base::flat_map<int, float> weights(
      predictor_->config_.bin_weights_map().begin(),
      predictor_->config_.bin_weights_map().end());
  // For bin 10
  SetLocalTime(1, 10);
  predictor_->Train(1u, 0u);
  predictor_->Train(1u, 0u);
  SetLocalTime(2, 10);
  predictor_->Train(2u, 0u);

  // For bin 11
  SetLocalTime(3, 11);
  predictor_->Train(1u, 0u);
  predictor_->Train(2u, 0u);
  // For bin 12
  SetLocalTime(5, 12);
  predictor_->Train(2u, 0u);

  // Train on weekend.
  SetLocalTime(6, 10);
  predictor_->Train(1u, 0u);
  predictor_->Train(2u, 0u);
  SetLocalTime(0, 11);
  predictor_->Train(2u, 0u);

  // Check workdays.
  SetLocalTime(1, 10);
  EXPECT_THAT(
      predictor_->Rank(0u),
      UnorderedElementsAre(
          Pair(1u, FloatEq((weights)[0] * 2.0 / 3.0 + weights[1] * 0.5)),
          Pair(2u, FloatEq(weights[0] * 1.0 / 3.0 + weights[1] * 0.5 +
                           weights[2] * 1.0))));

  // Check weekends.
  SetLocalTime(0, 9);
  EXPECT_THAT(predictor_->Rank(0u),
              UnorderedElementsAre(
                  Pair(1u, FloatEq(weights[1] * 1.0 / 2.0)),
                  Pair(2u, FloatEq(weights[1] * 1.0 / 2.0 + weights[2]))));
}

TEST_F(HourBinPredictorTest, FromProto) {
  RecurrencePredictorProto proto = MakeTestingProto();
  predictor_->FromProto(proto);
  SetLocalTime(1, 11);
  EXPECT_THAT(
      predictor_->Rank(0u),
      UnorderedElementsAre(Pair(1u, FloatEq(0.4125)), Pair(2u, FloatEq(0.0375)),
                           Pair(3u, FloatEq(0.3))));
}

TEST_F(HourBinPredictorTest, FromProtoDecays) {
  RecurrencePredictorProto proto = MakeTestingProto();
  proto.mutable_hour_bin_predictor()->set_last_decay_timestamp(350);
  predictor_->FromProto(proto);
  SetLocalTime(1, 11);
  EXPECT_THAT(predictor_->Rank(0u),
              UnorderedElementsAre(Pair(1u, FloatEq(0.15))));

  // Check if empty items got deleted during decay.
  EXPECT_EQ(
      static_cast<int>(predictor_->proto_.binned_frequency_table().size()), 1);
  EXPECT_EQ(static_cast<int>(
                (*predictor_->proto_.mutable_binned_frequency_table())[10]
                    .frequency()
                    .size()),
            1);
}

TEST_F(HourBinPredictorTest, ToProto) {
  RecurrencePredictorProto proto;
  SetLocalTime(1, 10);
  predictor_->Train(1u, 0u);
  predictor_->Train(1u, 0u);
  predictor_->Train(1u, 0u);
  predictor_->Train(2u, 0u);

  SetLocalTime(1, 11);
  predictor_->Train(1u, 0u);
  predictor_->Train(3u, 0u);
  predictor_->SetLastDecayTimestamp(365);

  predictor_->ToProto(&proto);
  RecurrencePredictorProto target_proto = MakeTestingProto();

  EXPECT_TRUE(proto.has_hour_bin_predictor());

  EXPECT_TRUE(EquivToProtoLite(proto.hour_bin_predictor(),
                               target_proto.hour_bin_predictor()));
}
}  // namespace app_list
