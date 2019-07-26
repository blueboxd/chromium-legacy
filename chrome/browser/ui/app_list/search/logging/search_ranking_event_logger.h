// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_LOGGING_SEARCH_RANKING_EVENT_LOGGER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_LOGGING_SEARCH_RANKING_EVENT_LOGGER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

class ChromeSearchResult;

namespace app_list {

// Performs UKM logging of search ranking events in the launcher's results list.
class SearchRankingEventLogger {
 public:
  SearchRankingEventLogger(Profile* profile,
                           SearchController* search_controller);
  ~SearchRankingEventLogger();
  // Called if a search result item got clicked, or a list of search result has
  // been shown to the user after a certain amount of time. |raw_query| is the
  // raw query that produced the results, |results| is a list of items that were
  // being shown to the users and their corresponding position indices of them
  // (see |SearchResultIdWithPositionIndex| for more details),
  // |position_index| is the position index of the clicked item (if no item got
  // clicked, |position_index| will be -1).
  void Log(const base::string16& trimmed_query,
           const ash::SearchResultIdWithPositionIndices& search_results,
           int launched_index);

  // Sets a testing-only closure to inform tests when a UKM event has been
  // recorded.
  void SetEventRecordedForTesting(base::OnceClosure closure);

 private:
  // Stores state necessary for logging a given search result that is
  // accumulated throughout the session.
  struct ResultState {
    ResultState();
    ~ResultState();

    base::Optional<base::Time> last_launch = base::nullopt;
    // Initialises all elements to 0.
    int launches_per_hour[24] = {};
    int launches_this_session = 0;
  };

  // Stores the relevant parts of a ChromeSearchResult used for logging.
  struct ResultInfo {
   public:
    ResultInfo();
    ResultInfo(const ResultInfo& other);
    ~ResultInfo();

    int index;
    std::string target;
    base::string16 title;
    ash::SearchResultType type;
    int subtype;
    float relevance;
  };

  // Calls the UKM API for a source ID relevant to |result|, and then begins the
  // logging process by calling LogEvent.
  void GetBackgroundSourceIdAndLogEvent(int event_id,
                                        const base::string16& trimmed_query,
                                        const ResultInfo& result_info,
                                        int launched_index);

  // Logs the given event to UKM. If |source_id| is nullopt then use a blank
  // source ID.
  void LogEvent(int event_id,
                const base::string16& trimmed_query,
                const ResultInfo& result_info,
                int launched_index,
                base::Optional<ukm::SourceId> source_id);

  SearchController* search_controller_;
  // Some events do not have an associated URL and so are logged directly with
  // |ukm_recorder_| using a blank source ID. Other events need to validate the
  // URL before recording, and use |ukm_background_recorder_|.
  ukm::UkmRecorder* ukm_recorder_;
  ukm::UkmBackgroundRecorderService* ukm_background_recorder_;

  // TODO(972817): Zero-state previous query results change their URL based on
  // the position they should be displayed at in the launcher. Because their
  // ID changes, we lose information on their result state. If, in future, we
  // want to rank these results and want more information, we should normalize
  // their IDs to remove the position information.
  std::map<std::string, ResultState> id_to_result_state_;

  // The next, unused, event ID.
  int next_event_id_ = 1;

  // Testing-only closure to inform tests when a UKM event has been recorded.
  base::OnceClosure event_recorded_for_testing_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SearchRankingEventLogger> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SearchRankingEventLogger);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_LOGGING_SEARCH_RANKING_EVENT_LOGGER_H_
