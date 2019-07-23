// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/ui/app_list/search/logging/search_ranking_event_logger.h"

namespace app_list {

SearchRankingEventLogger::SearchRankingEventLogger() {}
SearchRankingEventLogger::~SearchRankingEventLogger() = default;

void SearchRankingEventLogger::Log(
    const base::string16& trimmed_query,
    const ash::SearchResultIdWithPositionIndices& search_results,
    int position_index) {
  if (trimmed_query.empty()) {
    LogSuggestedZeroStateItems(search_results, position_index);
  } else {
    // TODO(crbug.com/972817): Add the logics for query-based events.
  }
}

void SearchRankingEventLogger::LogSuggestedZeroStateItems(
    const ash::SearchResultIdWithPositionIndices& search_results,
    int position_index) {
  // TODO(crbug.com/972817): Add the logics to log the suggested items.
}

}  // namespace app_list
