// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_SEARCH_RESULT_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_SEARCH_RESULT_RANKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/mixer.h"

namespace app_list {

class RecurrenceRanker;
enum class RankingItemType;

// SearchResultRanker re-ranks launcher search and zero-state results using a
// collection of on-device models. It can be provided training signals via the
// Train method, which are then forwarded to the appropriate model.
// FetchRankings queries each model for ranking results. Rank modifies the
// scores of provided search results, which are intended to be the output of a
// search provider.
class SearchResultRanker : file_manager::file_tasks::FileTasksObserver {
 public:
  explicit SearchResultRanker(Profile* profile);
  ~SearchResultRanker() override;

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
  // the SearchResultRanker. |id| is the string ID of an item that is launched
  // from the launcher, eg. an app ID or a filepath, and is derived from the
  // relevant ChromeSearchResult's ID.
  void Train(const std::string& id, RankingItemType type);

  // file_manager::file_tasks::FileTaskObserver:
  void OnFilesOpened(const std::vector<FileOpenEvent>& file_opens) override;

  RecurrenceRanker* get_zero_state_mixed_types_ranker();

 private:
  // Records the time of the last call to FetchRankings() and is used to
  // limit the number of queries to the models within a short timespan.
  base::Time time_of_last_fetch_;

  // How much the scores produced by |results_list_group_ranker_| affect the
  // final scores. Controlled by Finch.
  float results_list_boost_coefficient_ = 0.0f;

  // Stores the scores produced by |results_list_group_ranker_|.
  base::flat_map<std::string, float> group_ranks_;

  // Stores the scores produced by |query_based_mixed_types_ranker|.
  base::flat_map<std::string, float> query_mixed_ranks_;

  // The |results_list_group_ranker_| and |query_based_mixed_types_ranker_| are
  // models for two different experiments. Only one will be constructed.

  // A model that ranks groups (eg. 'file' and 'omnibox'), which is used to
  // tweak the results shown in the search results list only. This does not
  // affect apps.
  std::unique_ptr<RecurrenceRanker> results_list_group_ranker_;

  // Ranks items shown in the results list after a search query. Currently
  // these are local files and omnibox results.
  std::unique_ptr<RecurrenceRanker> query_based_mixed_types_ranker_;

  // Ranks files and previous queries for launcher zero-state.
  std::unique_ptr<RecurrenceRanker> zero_state_mixed_types_ranker_;

  // TODO(931149): Move the AppSearchResultRanker instance and associated logic
  // to here.

  Profile* profile_;

  const bool enable_zero_state_mixed_types_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_SEARCH_RESULT_RANKER_H_
