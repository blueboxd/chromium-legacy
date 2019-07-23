// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_LOGGING_SEARCH_RANKING_EVENT_LOGGER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_LOGGING_SEARCH_RANKING_EVENT_LOGGER_H_

#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/logging.h"
#include "base/time/time.h"

namespace app_list {

class SearchRankingEventLogger {
 public:
  SearchRankingEventLogger();
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
           int position_index);

 private:
  // Logs suggested items either from impressions or from click events.
  void LogSuggestedZeroStateItems(
      const ash::SearchResultIdWithPositionIndices& search_results,
      int position_index);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_LOGGING_SEARCH_RANKING_EVENT_LOGGER_H_
