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
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_search_result_ranker.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"
#include "url/gurl.h"

namespace app_list {
namespace {

using base::Time;
using base::TimeDelta;
using file_manager::file_tasks::FileTasksObserver;

// Limits how frequently models are queried for ranking results.
constexpr TimeDelta kMinSecondsBetweenFetches = TimeDelta::FromSeconds(1);

constexpr char kLogFileOpenType[] = "RecurrenceRanker.LogFileOpenType";

// Represents each model used within the SearchResultRanker.
enum class Model { NONE, APPS, MIXED_TYPES };

// Returns the model relevant for predicting launches for results with the given
// |type|.
Model ModelForType(RankingItemType type) {
  // TODO(959679): Update this with zero-state item types once zero-state
  // search providers have been implemented.
  switch (type) {
    case RankingItemType::kFile:
    case RankingItemType::kOmniboxGeneric:
    case RankingItemType::kOmniboxBookmark:
    case RankingItemType::kOmniboxDocument:
    case RankingItemType::kOmniboxHistory:
    case RankingItemType::kOmniboxSearch:
      return Model::MIXED_TYPES;
    // Currently we don't rank arc app shortcuts. If this changes, add a case
    // here.
    case RankingItemType::kApp:
      return Model::APPS;
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

// Performs any per-type normalization required on a search result ID. This is
// meant to simplify the space of IDs in cases where they are too sparse.
std::string NormalizeId(const std::string& id, RankingItemType type) {
  // Put any further normalizations here.
  switch (type) {
    case RankingItemType::kOmniboxGeneric:
    case RankingItemType::kOmniboxBookmark:
    case RankingItemType::kOmniboxDocument:
    case RankingItemType::kOmniboxHistory:
    case RankingItemType::kOmniboxSearch:
      // Heuristically check if the URL points to a Drive file. If so, strip
      // some extra information from it.
      if (GURL(id).host() == "docs.google.com")
        return SimplifyGoogleDocsUrlId(id);
      else
        return SimplifyUrlId(id);
    case RankingItemType::kApp:
      return NormalizeAppId(id);
    default:
      return id;
  }
}

// Linearly maps |score| to the range [min, max].
// |score| is assumed to be within [0.0, 1.0]; if it's greater than 1.0
// then max is returned; if it's less than 0.0, then min is returned.
float ReRange(const float score, const float min, const float max) {
  if (score >= 1.0f)
    return max;
  if (score <= 0.0f)
    return min;

  return min + score * (max - min);
}

}  // namespace

SearchResultRanker::SearchResultRanker(Profile* profile,
                                       history::HistoryService* history_service,
                                       service_manager::Connector* connector)
    : config_converter_(connector),
      history_service_observer_(this),
      profile_(profile),
      weak_factory_(this) {
  DCHECK(profile);
  DCHECK(history_service);
  history_service_observer_.Add(history_service);
  if (auto* notifier =
          file_manager::file_tasks::FileTasksNotifier::GetForProfile(
              profile_)) {
    notifier->AddObserver(this);
  }
}

SearchResultRanker::~SearchResultRanker() {
  if (auto* notifier =
          file_manager::file_tasks::FileTasksNotifier::GetForProfile(
              profile_)) {
    notifier->RemoveObserver(this);
  }
}

void SearchResultRanker::InitializeRankers() {
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
    config.mutable_predictor()->mutable_default_predictor();

    if (GetFieldTrialParamByFeatureAsBool(
            app_list_features::kEnableQueryBasedMixedTypesRanker,
            "use_category_model", false)) {
      // Group ranker model.
      results_list_group_ranker_ = std::make_unique<RecurrenceRanker>(
          "QueryBasedMixedTypesGroup",
          profile_->GetPath().AppendASCII("results_list_group_ranker.pb"),
          config, chromeos::ProfileHelper::IsEphemeralUserProfile(profile_));

    } else {
      // Item ranker model.
      const std::string config_json = GetFieldTrialParamValueByFeature(
          app_list_features::kEnableQueryBasedMixedTypesRanker, "config");
      config_converter_.Convert(
          config_json, "QueryBasedMixedTypes",
          base::BindOnce(
              [](SearchResultRanker* ranker,
                 const RecurrenceRankerConfigProto& default_config,
                 base::Optional<RecurrenceRankerConfigProto> parsed_config) {
                if (ranker->json_config_parsed_for_testing_)
                  std::move(ranker->json_config_parsed_for_testing_).Run();
                ranker->query_based_mixed_types_ranker_ =
                    std::make_unique<RecurrenceRanker>(
                        "QueryBasedMixedTypes",
                        ranker->profile_->GetPath().AppendASCII(
                            "query_based_mixed_types_ranker.pb"),
                        parsed_config ? parsed_config.value() : default_config,
                        chromeos::ProfileHelper::IsEphemeralUserProfile(
                            ranker->profile_));
              },
              base::Unretained(this), config));
    }
  }

  if (app_list_features::IsZeroStateMixedTypesRankerEnabled()) {
    zero_state_item_coeff_ = base::GetFieldTrialParamByFeatureAsDouble(
        app_list_features::kEnableZeroStateMixedTypesRanker, "item_coeff",
        zero_state_item_coeff_);
    zero_state_group_coeff_ = base::GetFieldTrialParamByFeatureAsDouble(
        app_list_features::kEnableZeroStateMixedTypesRanker, "group_coeff",
        zero_state_group_coeff_);
    zero_state_paired_coeff_ = base::GetFieldTrialParamByFeatureAsDouble(
        app_list_features::kEnableZeroStateMixedTypesRanker, "paired_coeff",
        zero_state_paired_coeff_);
    zero_state_default_group_score_ = base::GetFieldTrialParamByFeatureAsDouble(
        app_list_features::kEnableZeroStateMixedTypesRanker,
        "default_group_score", zero_state_default_group_score_);

    // TODO(959679): Setup json deployment.
    // TODO(959679): Swap to a stochastic model by default and tweak params.
    RecurrenceRankerConfigProto config;
    config.set_min_seconds_between_saves(240u);
    config.set_condition_limit(1u);
    config.set_condition_decay(0.5f);
    config.set_target_limit(5u);
    config.set_target_decay(0.8f);
    config.mutable_predictor()->mutable_default_predictor();

    zero_state_group_ranker_ = std::make_unique<RecurrenceRanker>(
        "ZeroStateGroups",
        profile_->GetPath().AppendASCII("zero_state_group_ranker.pb"), config,
        chromeos::ProfileHelper::IsEphemeralUserProfile(profile_));
  }

  if (app_list_features::IsAppRankerEnabled() &&
      GetFieldTrialParamByFeatureAsBool(app_list_features::kEnableAppRanker,
                                        "use_recurrence_ranker", true)) {
    RecurrenceRankerConfigProto config;
    config.set_min_seconds_between_saves(240u);
    config.set_condition_limit(1u);
    config.set_condition_decay(0.5);
    config.set_target_limit(200);
    config.set_target_decay(0.8);
    config.mutable_predictor()->mutable_default_predictor();

    app_ranker_ = std::make_unique<RecurrenceRanker>(
        "AppRanker", profile_->GetPath().AppendASCII("app_ranker.pb"), config,
        chromeos::ProfileHelper::IsEphemeralUserProfile(profile_));
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
  last_query_ = query;

  if (query.empty() && zero_state_group_ranker_) {
    zero_state_group_ranks_.clear();
    zero_state_group_ranks_ = zero_state_group_ranker_->Rank();
  }

  if (results_list_group_ranker_) {
    group_ranks_.clear();
    group_ranks_ = results_list_group_ranker_->Rank();
  } else if (query_based_mixed_types_ranker_) {
    query_mixed_ranks_.clear();
    query_mixed_ranks_ =
        query_based_mixed_types_ranker_->Rank(base::UTF16ToUTF8(query));
  }

  if (app_ranker_)
    app_ranks_ = app_ranker_->Rank();
}

void SearchResultRanker::Rank(Mixer::SortedResults* results) {
  if (!results)
    return;

  for (auto& result : *results) {
    const auto& type = RankingItemTypeFromSearchResult(*result.result);
    const auto& model = ModelForType(type);

    if (model == Model::MIXED_TYPES) {
      if (last_query_.empty() && zero_state_group_ranker_) {
        const auto& rank_it = zero_state_group_ranks_.find(
            base::NumberToString(static_cast<int>(type)));
        const float group_score = (rank_it != zero_state_group_ranks_.end())
                                      ? rank_it->second
                                      : zero_state_default_group_score_;
        const float score =
            zero_state_item_coeff_ * result.score +
            zero_state_group_coeff_ * group_score +
            zero_state_paired_coeff_ * result.score * group_score;
        result.score =
            score / (zero_state_item_coeff_ + zero_state_group_coeff_ +
                     zero_state_paired_coeff_);
      } else if (results_list_group_ranker_) {
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
        const auto& rank_it =
            query_mixed_ranks_.find(NormalizeId(result.result->id(), type));
        if (rank_it != query_mixed_ranks_.end()) {
          result.score = std::min(
              result.score + rank_it->second * results_list_boost_coefficient_,
              3.0);
        }
      }
    } else if (model == Model::APPS && app_ranker_) {
      const auto& it = app_ranks_.find(result.result->id());
      if (it != app_ranks_.end()) {
        result.score = ReRange(it->second, 0.67, 1.0);
      }
    }
  }
}

void SearchResultRanker::Train(const AppLaunchData& app_launch_data) {
  if (app_launch_data.launched_from ==
      ash::AppListLaunchedFrom::kLaunchedFromGrid) {
    // Log the AppResult from the grid to the UKM system.
    app_launch_event_logger_.OnGridClicked(app_launch_data.id);
  } else if (app_launch_data.launch_type ==
             ash::AppListLaunchType::kAppSearchResult) {
    // Log the AppResult (either in the search result page, or in chip form in
    // AppsGridView) to the UKM system.
    app_launch_event_logger_.OnSuggestionChipOrSearchBoxClicked(
        app_launch_data.id, app_launch_data.suggestion_index,
        static_cast<int>(app_launch_data.launched_from));
  }

  auto model = ModelForType(app_launch_data.ranking_item_type);
  if (model == Model::MIXED_TYPES) {
    if (app_launch_data.query.empty() && zero_state_group_ranker_) {
      zero_state_group_ranker_->Record(base::NumberToString(
          static_cast<int>(app_launch_data.ranking_item_type)));
    } else if (results_list_group_ranker_) {
      results_list_group_ranker_->Record(base::NumberToString(
          static_cast<int>(app_launch_data.ranking_item_type)));
    } else if (query_based_mixed_types_ranker_) {
      query_based_mixed_types_ranker_->Record(
          NormalizeId(app_launch_data.id, app_launch_data.ranking_item_type),
          app_launch_data.query);
    }
  } else if (model == Model::APPS && app_ranker_) {
    app_ranker_->Record(NormalizeAppId(app_launch_data.id));
  }
}

void SearchResultRanker::OnFilesOpened(
    const std::vector<FileOpenEvent>& file_opens) {
  // Log the open type of file open events
  for (const auto& file_open : file_opens)
    UMA_HISTOGRAM_ENUMERATION(kLogFileOpenType,
                              GetTypeFromFileTaskNotifier(file_open.open_type));
}

void SearchResultRanker::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (!query_based_mixed_types_ranker_)
    return;

  if (deletion_info.IsAllHistory()) {
    // TODO(931149): We clear the whole model because we expect most targets to
    // be URLs. In future, consider parsing the targets and only deleting URLs.
    query_based_mixed_types_ranker_->GetTargetData()->clear();
  } else {
    for (const auto& row : deletion_info.deleted_rows()) {
      // In order to perform URL normalization, NormalizeId requires any omnibox
      // item type as argument. Pass kOmniboxGeneric here as we don't know the
      // specific type.
      query_based_mixed_types_ranker_->RemoveTarget(
          NormalizeId(row.url().spec(), RankingItemType::kOmniboxGeneric));
    }
  }

  // Force a save to disk. It is possible to get many calls to OnURLsDeleted in
  // quick succession, eg. when all history is cleared from a different device.
  // So delay the save slightly and only perform one save for all updates during
  // that delay.
  if (!query_mixed_ranker_save_queued_) {
    query_mixed_ranker_save_queued_ = true;
    DCHECK(base::SequencedTaskRunnerHandle::IsSet());
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SearchResultRanker::SaveQueryMixedRankerAfterDelete,
                       weak_factory_.GetWeakPtr()),
        TimeDelta::FromSeconds(3));
  }
}

void SearchResultRanker::SaveQueryMixedRankerAfterDelete() {
  query_based_mixed_types_ranker_->SaveToDisk();
  query_mixed_ranker_save_queued_ = false;
}

}  // namespace app_list
