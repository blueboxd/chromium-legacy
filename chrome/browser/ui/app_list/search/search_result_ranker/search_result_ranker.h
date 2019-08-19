// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_SEARCH_RESULT_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_SEARCH_RESULT_RANKER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/mixer.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_data.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_event_logger.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_util.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"

namespace app_list {

class RecurrenceRanker;
enum class RankingItemType;

// SearchResultRanker re-ranks launcher search and zero-state results using a
// collection of on-device models. It can be provided training signals via the
// Train method, which are then forwarded to the appropriate model.
// FetchRankings queries each model for ranking results. Rank modifies the
// scores of provided search results, which are intended to be the output of a
// search provider.
class SearchResultRanker : file_manager::file_tasks::FileTasksObserver,
                           history::HistoryServiceObserver {
 public:
  SearchResultRanker(Profile* profile,
                     history::HistoryService* history_service,
                     service_manager::Connector* connector);
  ~SearchResultRanker() override;

  // Performs all setup of rankers. This is separated from the constructor for
  // testing reasons.
  void InitializeRankers();

  // Queries each model contained with the SearchResultRanker for its results,
  // and saves them for use on subsequent calls to Rank(). The given query may
  // be used as a feature for ranking search results provided to Rank(), but is
  // not used to create new search results. If this is a zero-state scenario,
  // the query should be empty.
  void FetchRankings(const base::string16& query);

  // Modifies the scores of |results| using the saved rankings. This should be
  // called after rankings have been queried with a call to FetchRankings().
  // Only the scores of elements in |results| are modified, not the
  // ChromeSearchResults themselves.
  void Rank(Mixer::SortedResults* results);

  // Forwards the given training signal to the relevant models contained within
  // the SearchResultRanker.
  void Train(const AppLaunchData& app_launch_data);

  // file_manager::file_tasks::FileTaskObserver:
  void OnFilesOpened(const std::vector<FileOpenEvent>& file_opens) override;

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  // Sets a testing-only closure to inform tests when a JSON config has been
  // parsed.
  void set_json_config_parsed_for_testing(base::OnceClosure closure) {
    json_config_parsed_for_testing_ = std::move(closure);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(SearchResultRankerTest,
                           QueryMixedModelConfigDeployment);
  FRIEND_TEST_ALL_PREFIXES(SearchResultRankerTest,
                           QueryMixedModelDeletesURLCorrectly);

  // Saves |query_based_mixed_types_ranker_| to disk. Called after a delay when
  // URLs get deleted.
  void SaveQueryMixedRankerAfterDelete();

  // Records the time of the last call to FetchRankings() and is used to
  // limit the number of queries to the models within a short timespan.
  base::Time time_of_last_fetch_;

  // The query last provided to FetchRankings.
  base::string16 last_query_;

  // How much the scores produced by |results_list_group_ranker_| affect the
  // final scores. Controlled by Finch.
  float results_list_boost_coefficient_ = 0.0f;

  // The |results_list_group_ranker_| and |query_based_mixed_types_ranker_| are
  // models for two different experiments. Only one will be constructed. Each
  // has an associated map used for caching its results.

  // A model that ranks groups (eg. 'file' and 'omnibox'), which is used to
  // tweak the results shown in the search results list only. This does not
  // affect apps.
  std::unique_ptr<RecurrenceRanker> results_list_group_ranker_;
  std::map<std::string, float> group_ranks_;

  // Ranks items shown in the results list after a search query. Currently
  // these are local files and omnibox results.
  std::unique_ptr<RecurrenceRanker> query_based_mixed_types_ranker_;
  std::map<std::string, float> query_mixed_ranks_;
  // Flag set when a delayed task to save the model is created. This is used to
  // prevent several delayed tasks from being created.
  bool query_mixed_ranker_save_queued_ = false;

  // Ranks the kinds of results possible in the zero state results list.
  std::unique_ptr<RecurrenceRanker> zero_state_group_ranker_;
  std::map<std::string, float> zero_state_group_ranks_;

  // Coefficients that control the weighting between different parts of the
  // score of a result in the zero-state results list.
  float zero_state_item_coeff_ = 1.0f;
  float zero_state_group_coeff_ = 0.0f;
  float zero_state_paired_coeff_ = 0.0f;
  float zero_state_default_group_score_ = 1.0f;

  // Ranks apps.
  std::unique_ptr<RecurrenceRanker> app_ranker_;
  std::map<std::string, float> app_ranks_;

  // Converts JSON config strings to RecurrenceRankerConfigProtos.
  JsonConfigConverter config_converter_;

  // Testing-only closure to inform tests once a JSON config has been parsed.
  base::OnceClosure json_config_parsed_for_testing_;

  // Logs launch events and stores feature data for aggregated model.
  std::unique_ptr<app_list::AppLaunchEventLogger> app_launch_event_logger_;

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_;

  Profile* profile_;

  base::WeakPtrFactory<SearchResultRanker> weak_factory_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_SEARCH_RESULT_RANKER_H_
