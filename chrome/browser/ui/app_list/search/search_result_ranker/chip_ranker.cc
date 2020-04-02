// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/chip_ranker.h"

#include <algorithm>
#include <string>
#include <utility>

#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_search_result_ranker.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"

namespace app_list {
namespace {

constexpr int kNumChips = 5;
constexpr char kApp[] = "app";
constexpr char kFile[] = "file";

// A small number that we expect to be smaller than the difference between the
// scores of any two results. This means it can be used to insert a result A
// between results B and C by setting A's score to B's score + kScoreEpsilon.
constexpr float kScoreEpsilon = 1e-5f;

void SortHighToLow(std::vector<Mixer::SortData*>* results) {
  std::sort(results->begin(), results->end(),
            [](const Mixer::SortData* const a, const Mixer::SortData* const b) {
              return a->score > b->score;
            });
}

float GetScore(const std::map<std::string, float>& scores,
               const std::string& key) {
  const auto it = scores.find(key);
  // We expect to always find a score for |key| in |scores|, because the ranker
  // is initialized with some default scores. However a state without scores is
  // possible, eg. if the recurrence ranker file is corrupted. In this case,
  // default a score to 1.
  if (it == scores.end()) {
    return 1.0f;
  }
  return it->second;
}

void InitializeRanker(RecurrenceRanker* ranker) {
  // This initialization puts two files and three apps in the chips.
  ranker->Record(kFile);
  ranker->Record(kFile);
  ranker->Record(kApp);
  ranker->Record(kApp);
  ranker->Record(kApp);
}

}  // namespace

ChipRanker::ChipRanker(Profile* profile) : profile_(profile) {
  DCHECK(profile);

  // Set up ranker model.
  RecurrenceRankerConfigProto config;
  config.set_min_seconds_between_saves(240u);
  config.set_condition_limit(1u);
  config.set_condition_decay(0.5f);
  config.set_target_limit(5u);
  config.set_target_decay(0.95f);
  config.mutable_predictor()->mutable_default_predictor();

  type_ranker_ = std::make_unique<RecurrenceRanker>(
      "", profile_->GetPath().AppendASCII("suggested_files_ranker.pb"), config,
      chromeos::ProfileHelper::IsEphemeralUserProfile(profile_));
}

ChipRanker::~ChipRanker() = default;

void ChipRanker::Train(const AppLaunchData& app_launch_data) {
  const auto type = app_launch_data.ranking_item_type;
  if (type == RankingItemType::kApp) {
    type_ranker_->Record(kApp);
  } else if (type == RankingItemType::kChip ||
             type == RankingItemType::kZeroStateFile ||
             type == RankingItemType::kDriveQuickAccess) {
    type_ranker_->Record(kFile);
  }
}

void ChipRanker::Rank(Mixer::SortedResults* results) {
  // Construct two lists of pointers, containing file chip and app results
  // respectively, sorted in decreasing order of score.
  std::vector<Mixer::SortData*> app_results;
  std::vector<Mixer::SortData*> file_results;
  for (auto& result : *results) {
    if (RankingItemTypeFromSearchResult(*result.result) ==
        RankingItemType::kApp) {
      app_results.emplace_back(&result);
    } else if (RankingItemTypeFromSearchResult(*result.result) ==
               RankingItemType::kChip) {
      file_results.emplace_back(&result);
    }
  }
  SortHighToLow(&app_results);
  SortHighToLow(&file_results);

  // The chip ranker only has work to do if both apps and files are present.
  if (app_results.empty() || file_results.empty())
    return;

  // If this is the first initialization of the ranker, warm it up with some
  // default scores for apps and files.
  if (type_ranker_->empty()) {
    InitializeRanker(type_ranker_.get());
  }

  // Get the two type scores from the ranker.
  const auto ranks = type_ranker_->Rank();
  float app_score = GetScore(ranks, kApp);
  float file_score = GetScore(ranks, kFile);
  const float score_delta = (file_score + app_score) / kNumChips;

  // Tweak file result scores to fit in with app scores. See header comment for
  // ChipRanker::Rank for more details.
  const int num_apps = static_cast<int>(app_results.size());
  const int num_files = static_cast<int>(file_results.size());
  int current_app = 0;
  int current_file = 0;
  for (int i = 0; i < kNumChips; ++i) {
    if (app_score > file_score) {
      app_score -= score_delta;
      ++current_app;
    } else if (current_file < num_files && current_app < num_apps) {
      file_results[current_file]->score =
          app_results[current_app]->score + kScoreEpsilon;
      ++current_file;
      file_score -= score_delta;
    }
  }
}

RecurrenceRanker* ChipRanker::GetRankerForTest() {
  return type_ranker_.get();
}

}  // namespace app_list
