// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_util.h"

#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_config.pb.h"

namespace app_list {

std::unique_ptr<RecurrencePredictor> MakePredictor(
    const RecurrencePredictorConfigProto& config) {
  if (config.has_fake_predictor())
    return std::make_unique<FakePredictor>(config.fake_predictor());
  if (config.has_default_predictor())
    return std::make_unique<DefaultPredictor>(config.default_predictor());
  if (config.has_conditional_frequency_predictor())
    return std::make_unique<ConditionalFrequencyPredictor>(
        config.conditional_frequency_predictor());
  if (config.has_frecency_predictor())
    return std::make_unique<FrecencyPredictor>(config.frecency_predictor());
  if (config.has_hour_bin_predictor())
    return std::make_unique<HourBinPredictor>(config.hour_bin_predictor());
  if (config.has_markov_predictor())
    return std::make_unique<MarkovPredictor>(config.markov_predictor());

  LogConfigurationError(ConfigurationError::kInvalidPredictor);
  NOTREACHED();
  return nullptr;
}

}  // namespace app_list
