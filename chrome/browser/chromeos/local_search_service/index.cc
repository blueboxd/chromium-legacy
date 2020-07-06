// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/index.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"

namespace local_search_service {

namespace {

// Only logs metrics if |histogram_prefix| is not empty.
void MaybeLogIndexIdAndBackendType(const std::string& histogram_prefix,
                                   Backend backend) {
  if (histogram_prefix.empty())
    return;

  base::UmaHistogramEnumeration(histogram_prefix + ".Backend", backend);
}

std::string IndexIdBasedHistogramPrefix(IndexId index_id) {
  const std::string prefix = "LocalSearchService.";
  switch (index_id) {
    case IndexId::kCrosSettings:
      return prefix + "CrosSettings";
    case IndexId::kHelpApp:
      return prefix + "HelpApp";
    default:
      return "";
  }
}

}  // namespace

Index::Index(IndexId index_id, Backend backend) {
  histogram_prefix_ = IndexIdBasedHistogramPrefix(index_id);
  if (!g_browser_process || !g_browser_process->local_state()) {
    return;
  }

  reporter_ =
      std::make_unique<SearchMetricsReporter>(g_browser_process->local_state());
  DCHECK(reporter_);
  reporter_->SetIndexId(index_id);

  MaybeLogIndexIdAndBackendType(histogram_prefix_, backend);
}

Index::~Index() = default;

void Index::MaybeLogSearchResultsStats(ResponseStatus status,
                                       size_t num_results) {
  if (reporter_)
    reporter_->OnSearchPerformed();

  if (histogram_prefix_.empty())
    return;

  base::UmaHistogramEnumeration(histogram_prefix_ + ".ResponseStatus", status);
  if (status == ResponseStatus::kSuccess) {
    // Only logs number of results if search is a success.
    base::UmaHistogramCounts100(histogram_prefix_ + ".NumberResults",
                                num_results);
  }
}

void Index::SetSearchParams(const SearchParams& search_params) {
  search_params_ = search_params;
}

SearchParams Index::GetSearchParamsForTesting() {
  return search_params_;
}


}  // namespace local_search_service
