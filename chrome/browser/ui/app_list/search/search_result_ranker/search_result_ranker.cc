// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/search_result_ranker.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"

namespace app_list {
namespace {

using base::Time;
using base::TimeDelta;
using file_manager::file_tasks::FileTasksObserver;

// Limits how frequently models are queried for ranking results.
constexpr TimeDelta kMinSecondsBetweenFetches = TimeDelta::FromSeconds(1);

constexpr char kLogFileOpenType[] = "RecurrenceRanker.LogFileOpenType";

// Represents each model used within the SearchResultRanker.
enum class Model { NONE, MIXED_TYPES };

// Returns the model relevant for predicting launches for results with the given
// |type|.
Model ModelForType(RankingItemType type) {
  switch (type) {
    case RankingItemType::kFile:
    case RankingItemType::kOmniboxGeneric:
    case RankingItemType::kOmniboxBookmark:
    case RankingItemType::kOmniboxDocument:
    case RankingItemType::kOmniboxHistory:
    case RankingItemType::kOmniboxSearch:
      return Model::MIXED_TYPES;
    default:
      return Model::NONE;
  }
}

// Represents various open types of file open events. These values persist to
// logs. Entries should not be renumbered and numeric values should never
// be reused.
enum class FileOpenType {
  kUnknown = 0,
  kLaunch = 1,
  kOpen = 2,
  kSaveAs = 3,
  kDownload = 4,
  kMaxValue = kDownload,
};

FileOpenType GetTypeFromFileTaskNotifier(FileTasksObserver::OpenType type) {
  switch (type) {
    case FileTasksObserver::OpenType::kLaunch:
      return FileOpenType::kLaunch;
    case FileTasksObserver::OpenType::kOpen:
      return FileOpenType::kOpen;
    case FileTasksObserver::OpenType::kSaveAs:
      return FileOpenType::kSaveAs;
    case FileTasksObserver::OpenType::kDownload:
      return FileOpenType::kDownload;
    default:
      return FileOpenType::kUnknown;
  }
}

}  // namespace

SearchResultRanker::SearchResultRanker(Profile* profile)
    : enable_zero_state_mixed_types_(
          app_list_features::IsZeroStateMixedTypesRankerEnabled()) {
  if (app_list_features::IsQueryBasedMixedTypesRankerEnabled()) {
    results_list_boost_coefficient_ = base::GetFieldTrialParamByFeatureAsDouble(
        app_list_features::kEnableQueryBasedMixedTypesRanker,
        "boost_coefficient", 0.1);

    RecurrenceRankerConfigProto config;

    config.set_min_seconds_between_saves(240u);
    config.set_condition_limit(0u);
    config.set_condition_decay(0.5f);
    config.set_target_limit(base::GetFieldTrialParamByFeatureAsInt(
        app_list_features::kEnableQueryBasedMixedTypesRanker, "target_limit",
        200));
    config.set_target_decay(base::GetFieldTrialParamByFeatureAsDouble(
        app_list_features::kEnableQueryBasedMixedTypesRanker, "target_decay",
        0.8f));
    // TODO(931149): Replace this with a more sophisticated model if the
    // query-based mixed type model is being used.
    config.mutable_default_predictor();

    if (GetFieldTrialParamByFeatureAsBool(
            app_list_features::kEnableQueryBasedMixedTypesRanker,
            "use_category_model", false)) {
      results_list_group_ranker_ = std::make_unique<RecurrenceRanker>(
          profile->GetPath().AppendASCII("results_list_group_ranker.pb"),
          config, chromeos::ProfileHelper::IsEphemeralUserProfile(profile));
    } else {
      query_based_mixed_types_ranker_ = std::make_unique<RecurrenceRanker>(
          profile->GetPath().AppendASCII("query_based_mixed_types_ranker.pb"),
          config, chromeos::ProfileHelper::IsEphemeralUserProfile(profile));
    }
  }

  profile_ = profile;
  if (auto* notifier =
          file_manager::file_tasks::FileTasksNotifier::GetForProfile(
              profile_)) {
    notifier->AddObserver(this);
  }

  if (enable_zero_state_mixed_types_) {
    RecurrenceRankerConfigProto config;
    config.set_min_seconds_between_saves(240u);
    config.set_condition_limit(0u);
    config.set_condition_decay(0.5f);

    config.set_target_limit(base::GetFieldTrialParamByFeatureAsInt(
        app_list_features::kEnableZeroStateMixedTypesRanker, "target_limit",
        200));
    config.set_target_decay(base::GetFieldTrialParamByFeatureAsDouble(
        app_list_features::kEnableZeroStateMixedTypesRanker, "target_decay",
        0.8f));

    // Despite not changing any fields, this sets the predictor to the default
    // predictor.
    config.mutable_default_predictor();

    zero_state_mixed_types_ranker_ = std::make_unique<RecurrenceRanker>(
        profile->GetPath().AppendASCII("zero_state_mixed_types_ranker.proto"),
        config, chromeos::ProfileHelper::IsEphemeralUserProfile(profile));
  }
}

SearchResultRanker::~SearchResultRanker() {
  if (auto* notifier =
          file_manager::file_tasks::FileTasksNotifier::GetForProfile(
              profile_)) {
    notifier->RemoveObserver(this);
  }
}

void SearchResultRanker::FetchRankings(const base::string16& query) {
  // The search controller potentially calls SearchController::FetchResults
  // several times for each user's search, so we cache the results of querying
  // the models for a short time, to prevent uneccessary queries.
  const auto& now = Time::Now();
  if (now - time_of_last_fetch_ < kMinSecondsBetweenFetches)
    return;
  time_of_last_fetch_ = now;

  // TODO(931149): The passed |query| should be used to choose between ranking
  // results with using a zero-state or query-based model.

  if (results_list_group_ranker_) {
    group_ranks_.clear();
    group_ranks_ = results_list_group_ranker_->Rank();
  } else if (query_based_mixed_types_ranker_) {
    query_mixed_ranks_.clear();
    query_mixed_ranks_ = query_based_mixed_types_ranker_->Rank();
  }
}

void SearchResultRanker::Rank(Mixer::SortedResults* results) {
  if (!results)
    return;

  for (auto& result : *results) {
    const auto& type = RankingItemTypeFromSearchResult(*result.result);
    const auto& model = ModelForType(type);

    if (model == Model::MIXED_TYPES) {
      if (results_list_group_ranker_) {
        const auto& rank_it =
            group_ranks_.find(base::NumberToString(static_cast<int>(type)));
        // The ranker only contains entries trained with types relating to files
        // or the omnibox. This means scores for apps, app shortcuts, and answer
        // cards will be unchanged.
        if (rank_it != group_ranks_.end()) {
          // Ranker scores are guaranteed to be in [0,1]. But, enforce that the
          // result of tweaking does not put the score above 3.0, as that may
          // interfere with apps or answer cards.
          result.score = std::min(
              result.score + rank_it->second * results_list_boost_coefficient_,
              3.0);
        }
      } else if (query_based_mixed_types_ranker_) {
        // TODO(931149): Add some normalization for URLs.
        const auto& rank_it = query_mixed_ranks_.find(result.result->id());
        if (rank_it != query_mixed_ranks_.end()) {
          result.score = std::min(
              result.score + rank_it->second * results_list_boost_coefficient_,
              3.0);
        }
      }
    }
  }
}

void SearchResultRanker::Train(const std::string& id, RankingItemType type) {
  if (ModelForType(type) == Model::MIXED_TYPES) {
    // TODO(931149): Add some normalization for URLs.
    if (results_list_group_ranker_) {
      results_list_group_ranker_->Record(
          base::NumberToString(static_cast<int>(type)));
    } else if (query_based_mixed_types_ranker_) {
      query_based_mixed_types_ranker_->Record(id);
    }
  }
}

void SearchResultRanker::OnFilesOpened(
    const std::vector<FileOpenEvent>& file_opens) {
  if (enable_zero_state_mixed_types_) {
    DCHECK(zero_state_mixed_types_ranker_);
    for (const auto& file_open : file_opens)
      zero_state_mixed_types_ranker_->Record(file_open.path.value());
  }
  // Log the open type of file open events
  for (const auto& file_open : file_opens)
    UMA_HISTOGRAM_ENUMERATION(kLogFileOpenType,
                              GetTypeFromFileTaskNotifier(file_open.open_type));
}

RecurrenceRanker* SearchResultRanker::get_zero_state_mixed_types_ranker() {
  return zero_state_mixed_types_ranker_.get();
}

}  // namespace app_list
