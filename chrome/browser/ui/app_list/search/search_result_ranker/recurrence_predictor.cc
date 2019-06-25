// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.pb.h"

namespace app_list {
namespace {

constexpr int kHoursADay = 24;

}  // namespace

FakePredictor::FakePredictor(const FakePredictorConfig& config) {
  // The fake predictor should only be used for testing, not in production.
  // Record an error so we know if it is being used.
  LogConfigurationError(ConfigurationError::kFakePredictorUsed);
}

FakePredictor::~FakePredictor() = default;

const char FakePredictor::kPredictorName[] = "FakePredictor";
const char* FakePredictor::GetPredictorName() const {
  return kPredictorName;
}

void FakePredictor::Train(unsigned int target, unsigned int condition) {
  counts_[target] += 1.0f;
}

base::flat_map<unsigned int, float> FakePredictor::Rank(
    unsigned int condition) {
  return counts_;
}

void FakePredictor::ToProto(RecurrencePredictorProto* proto) const {
  auto* counts = proto->mutable_fake_predictor()->mutable_counts();
  for (auto& pair : counts_)
    (*counts)[pair.first] = pair.second;
}

void FakePredictor::FromProto(const RecurrencePredictorProto& proto) {
  if (!proto.has_fake_predictor()) {
    LogSerializationError(SerializationError::kFakePredictorLoadingError);
    return;
  }

  for (const auto& pair : proto.fake_predictor().counts())
    counts_[pair.first] = pair.second;
}

DefaultPredictor::DefaultPredictor(const DefaultPredictorConfig& config) {}
DefaultPredictor::~DefaultPredictor() {}

void DefaultPredictor::Train(unsigned int target, unsigned int condition) {}

base::flat_map<unsigned int, float> DefaultPredictor::Rank(
    unsigned int condition) {
  LogUsageError(UsageError::kInvalidRankCall);
  NOTREACHED();
  return {};
}

const char DefaultPredictor::kPredictorName[] = "DefaultPredictor";
const char* DefaultPredictor::GetPredictorName() const {
  return kPredictorName;
}

void DefaultPredictor::ToProto(RecurrencePredictorProto* proto) const {}

void DefaultPredictor::FromProto(const RecurrencePredictorProto& proto) {}

FrecencyPredictor::FrecencyPredictor(const FrecencyPredictorConfig& config)
    : decay_coeff_(config.decay_coeff()) {}
FrecencyPredictor::~FrecencyPredictor() = default;

const char FrecencyPredictor::kPredictorName[] = "FrecencyPredictor";
const char* FrecencyPredictor::GetPredictorName() const {
  return kPredictorName;
}

void FrecencyPredictor::Train(unsigned int target, unsigned int condition) {
  ++num_updates_;
  TargetData& data = targets_[target];
  DecayScore(&data);
  data.last_score += 1.0f - decay_coeff_;
}

base::flat_map<unsigned int, float> FrecencyPredictor::Rank(
    unsigned int condition) {
  base::flat_map<unsigned int, float> result;
  for (auto& pair : targets_) {
    DecayScore(&pair.second);
    result[pair.first] = pair.second.last_score;
  }
  return result;
}

void FrecencyPredictor::ToProto(RecurrencePredictorProto* proto) const {
  auto* predictor = proto->mutable_frecency_predictor();

  predictor->set_num_updates(num_updates_);

  for (const auto& pair : targets_) {
    auto* target_data = predictor->add_targets();
    target_data->set_id(pair.first);
    target_data->set_last_score(pair.second.last_score);
    target_data->set_last_num_updates(pair.second.last_num_updates);
  }
}

void FrecencyPredictor::FromProto(const RecurrencePredictorProto& proto) {
  if (!proto.has_frecency_predictor()) {
    LogSerializationError(SerializationError::kFrecencyPredictorLoadingError);
    return;
  }
  const auto& predictor = proto.frecency_predictor();

  num_updates_ = predictor.num_updates();

  base::flat_map<unsigned int, TargetData> targets;
  for (const auto& target_data : predictor.targets()) {
    targets[target_data.id()] = {target_data.last_score(),
                                 target_data.last_num_updates()};
  }
  targets_.swap(targets);
}

void FrecencyPredictor::DecayScore(TargetData* data) {
  int time_since_update = num_updates_ - data->last_num_updates;

  if (time_since_update > 0) {
    data->last_score *= std::pow(decay_coeff_, time_since_update);
    data->last_num_updates = num_updates_;
  }
}

HourBinPredictor::HourBinPredictor(const HourBinPredictorConfig& config)
    : weekly_decay_coeff_(config.weekly_decay_coeff()) {
  for (const auto& pair : config.bin_weights())
    bin_weights_[pair.bin()] = pair.weight();

  if (!proto_.has_last_decay_timestamp())
    SetLastDecayTimestamp(
        base::Time::Now().ToDeltaSinceWindowsEpoch().InDays());
}

HourBinPredictor::~HourBinPredictor() = default;

const char HourBinPredictor::kPredictorName[] = "HourBinPredictor";

const char* HourBinPredictor::GetPredictorName() const {
  return kPredictorName;
}

int HourBinPredictor::GetBinFromHourDifference(int hour_difference) const {
  base::Time shifted_time =
      base::Time::Now() + base::TimeDelta::FromHours(hour_difference);
  base::Time::Exploded exploded_time;
  shifted_time.LocalExplode(&exploded_time);

  const bool is_weekend =
      exploded_time.day_of_week == 6 || exploded_time.day_of_week == 0;

  // To distinguish workdays from weekend, use now.hour for workdays and
  // now.hour + 24 for weekend.
  if (!is_weekend) {
    return exploded_time.hour;
  } else {
    return exploded_time.hour + kHoursADay;
  }
}

int HourBinPredictor::GetBin() const {
  return GetBinFromHourDifference(0);
}

void HourBinPredictor::Train(unsigned int target, unsigned int condition) {
  int hour = GetBin();
  auto& frequency_table = (*proto_.mutable_binned_frequency_table())[hour];
  frequency_table.set_total_counts(frequency_table.total_counts() + 1);
  (*frequency_table.mutable_frequency())[target] += 1;
}

base::flat_map<unsigned int, float> HourBinPredictor::Rank(
    unsigned int condition) {
  base::flat_map<unsigned int, float> ranks;
  const auto& frequency_table_map = proto_.binned_frequency_table();
  for (const auto& hour_and_weight : bin_weights_) {
    // Find adjacent bin and weight.
    const int adj_bin = GetBinFromHourDifference(hour_and_weight.first);
    const float weight = hour_and_weight.second;

    const auto find_frequency_table = frequency_table_map.find(adj_bin);
    if (find_frequency_table == frequency_table_map.end())
      continue;
    const auto& frequency_table = find_frequency_table->second;

    // Accumulates the frequency to the output.
    if (frequency_table.total_counts() > 0) {
      const int total_counts = frequency_table.total_counts();
      for (const auto& pair : frequency_table.frequency()) {
        ranks[pair.first] +=
            static_cast<float>(pair.second) / total_counts * weight;
      }
    }
  }
  return ranks;
}

void HourBinPredictor::ToProto(RecurrencePredictorProto* proto) const {
  *proto->mutable_hour_bin_predictor() = proto_;
}

void HourBinPredictor::FromProto(const RecurrencePredictorProto& proto) {
  if (!proto.has_hour_bin_predictor())
    return;

  proto_ = proto.hour_bin_predictor();
  if (ShouldDecay())
    DecayAll();
}

bool HourBinPredictor::ShouldDecay() {
  const int today = base::Time::Now().ToDeltaSinceWindowsEpoch().InDays();
  // Check if we should decay the frequency
  return today - proto_.last_decay_timestamp() > 7;
}

void HourBinPredictor::DecayAll() {
  SetLastDecayTimestamp(base::Time::Now().ToDeltaSinceWindowsEpoch().InDays());
  auto& frequency_table_map = *proto_.mutable_binned_frequency_table();
  for (auto it_table = frequency_table_map.begin();
       it_table != frequency_table_map.end();) {
    auto& frequency_table = *it_table->second.mutable_frequency();
    for (auto it_freq = frequency_table.begin();
         it_freq != frequency_table.end();) {
      const int new_frequency = it_freq->second * weekly_decay_coeff_;
      it_table->second.set_total_counts(it_table->second.total_counts() -
                                        it_freq->second + new_frequency);
      it_freq->second = new_frequency;

      // Remove item that has zero frequency
      if (it_freq->second == 0) {
        frequency_table.erase(it_freq++);
      } else {
        it_freq++;
      }
    }

    // Remove bin that has zero total_counts
    if (it_table->second.total_counts() == 0) {
      frequency_table_map.erase(it_table++);
    } else {
      it_table++;
    }
  }
}

}  // namespace app_list
