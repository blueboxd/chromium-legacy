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

// Apps have a boost of 8.0 + app ranker score in range [0, 1],
// hence the range of scores is [8.0, 9.0].
constexpr double kScoreHi = 9.0;
constexpr double kScoreLo = 8.0;

// Returns whether the model should be trained on this type of data.
bool ShouldTrain(RankingItemType type) {
  switch (type) {
    case RankingItemType::kApp:
    case RankingItemType::kChip:
    case RankingItemType::kZeroStateFile:
    case RankingItemType::kDriveQuickAccess:
      return true;
    default:
      return false;
  }
}

double FetchScore(const std::map<std::string, float> ranks,
                  ChromeSearchResult* r) {
  const auto it = ranks.find(NormalizeAppId(r->id()));
  if (it != ranks.end())
    return it->second;
  return 0.0;
}

int GetNextMatchingIndex(
    Mixer::SortedResults* results,
    const base::RepeatingCallback<bool(const ChromeSearchResult*)>&
        result_filter,
    int from_index) {
  int i = from_index + 1;
  while (i < static_cast<int>(results->size())) {
    if (result_filter.Run((*results)[i].result)) {
      return i;
    }
    ++i;
  }
  return -1;
}

}  // namespace

ChipRanker::ChipRanker(Profile* profile) : profile_(profile) {
  DCHECK(profile);
  // Set up ranker model.
  RecurrenceRankerConfigProto config;
  config.set_min_seconds_between_saves(240u);
  config.set_condition_limit(1u);
  config.set_condition_decay(0.6f);
  config.set_target_limit(200);
  config.set_target_decay(0.9f);
  config.mutable_predictor()->mutable_default_predictor();

  ranker_ = std::make_unique<RecurrenceRanker>(
      "", profile_->GetPath().AppendASCII("suggested_files_ranker.pb"), config,
      chromeos::ProfileHelper::IsEphemeralUserProfile(profile_));
}

ChipRanker::~ChipRanker() = default;

void ChipRanker::Train(const AppLaunchData& app_launch_data) {
  // ID normalisation will ensure that a file launched from the zero-state
  // result list is counted as the same item as the same file launched from
  // the suggestion chips.
  if (ShouldTrain(app_launch_data.ranking_item_type)) {
    ranker_->Record(NormalizeAppId(app_launch_data.id));
  }
}

void ChipRanker::Rank(Mixer::SortedResults* results) {
  std::sort(results->begin(), results->end());

  const auto app_chip_filter =
      base::BindRepeating([](const ChromeSearchResult* r) -> bool {
        return (r->display_type() == ash::SearchResultDisplayType::kTile ||
                r->display_type() == ash::SearchResultDisplayType::kChip) &&
               r->is_recommendation();
      });

  const auto file_chip_filter =
      base::BindRepeating([](const ChromeSearchResult* r) -> bool {
        return r->result_type() == ash::AppListSearchResultType::kFileChip ||
               r->result_type() ==
                   ash::AppListSearchResultType::kDriveQuickAccessChip;
      });

  // Use filters to find first two app chips and first file chip
  int app1 = GetNextMatchingIndex(results, app_chip_filter, -1);
  int app2 = GetNextMatchingIndex(results, app_chip_filter, app1);
  int file = GetNextMatchingIndex(results, file_chip_filter, -1);
  int prev_file = -1;

  // If we couldn't find any files or couldn't find two or more apps.
  if (file < 0 || app1 < 0 || app2 < 0) {
    return;
  }

  // Fetch rankings from |ranker_|.
  std::map<std::string, float> ranks = ranker_->Rank();

  // Refer to class comment.
  double app1_rescore = FetchScore(ranks, (*results)[app1].result);
  double app2_rescore = FetchScore(ranks, (*results)[app2].result);
  double file_rescore = 0.0;
  double prev_file_rescore = kScoreHi;
  double hi = 0.0;
  double lo = 0.0;

  while (file >= 0 && app1 >= 0) {
    file_rescore = FetchScore(ranks, (*results)[file].result);

    // File should sit above lowest of two app scores.
    if (file_rescore > app2_rescore) {
      // Find upper and lower bounds on score.
      hi = prev_file > 0 ? (*results)[prev_file].score : kScoreHi;
      lo = app2 > 0 ? (*results)[app2].score : kScoreLo;

      if (prev_file_rescore > app1_rescore) {
        if (file_rescore < app1_rescore)
          hi = (*results)[app1].score;
        else if (file_rescore > app1_rescore)
          lo = (*results)[app1].score;
      }

      // Place new score at midpoint between hi and lo.
      (*results)[file].score = lo + ((hi - lo) / 2);

      prev_file = file;
      file = GetNextMatchingIndex(results, file_chip_filter, file);
      prev_file_rescore = file_rescore;

    } else {
      // File should sit below both current app scores.
      app1 = app2;
      app1_rescore = app2_rescore;
      app2 = GetNextMatchingIndex(results, app_chip_filter, app1);
      app2_rescore =
          app2 < 0 ? kScoreLo : FetchScore(ranks, (*results)[app2].result);
    }
  }
}

void ChipRanker::SetForTest(std::unique_ptr<RecurrenceRanker> ranker) {
  ranker_ = std::move(ranker);
}

}  // namespace app_list
