// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"

#include <cmath>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"

namespace app_list {
namespace {

ZeroStateResultType ZeroStateTypeFromRankingType(
    RankingItemType ranking_item_type) {
  switch (ranking_item_type) {
    case RankingItemType::kUnknown:
      return ZeroStateResultType::kUnknown;
    case RankingItemType::kIgnored:
    case RankingItemType::kFile:
    case RankingItemType::kApp:
    case RankingItemType::kArcAppShortcut:
    case RankingItemType::kChip:
      return ZeroStateResultType::kUnanticipated;
    case RankingItemType::kOmniboxGeneric:
      return ZeroStateResultType::kOmniboxSearch;
    case RankingItemType::kZeroStateFile:
      return ZeroStateResultType::kZeroStateFile;
    case RankingItemType::kDriveQuickAccess:
      return ZeroStateResultType::kDriveQuickAccess;
  }
}

}  // namespace

void LogInitializationStatus(const std::string& suffix,
                             InitializationStatus status) {
  if (suffix.empty())
    return;
  base::UmaHistogramEnumeration(
      "RecurrenceRanker.InitializationStatus." + suffix, status);
}

void LogSerializationStatus(const std::string& suffix,
                            SerializationStatus status) {
  if (suffix.empty())
    return;
  base::UmaHistogramEnumeration(
      "RecurrenceRanker.SerializationStatus." + suffix, status);
}

void LogUsage(const std::string& suffix, Usage usage) {
  if (suffix.empty())
    return;
  base::UmaHistogramEnumeration("RecurrenceRanker.Usage." + suffix, usage);
}

void LogJsonConfigConversionStatus(const std::string& suffix,
                                   JsonConfigConversionStatus status) {
  if (suffix.empty())
    return;
  base::UmaHistogramEnumeration(
      "RecurrenceRanker.JsonConfigConversion." + suffix, status);
}

void LogZeroStateLaunchType(RankingItemType ranking_item_type) {
  const auto zero_state_type = ZeroStateTypeFromRankingType(ranking_item_type);
  UMA_HISTOGRAM_ENUMERATION("Apps.AppList.ZeroStateResults.LaunchedItemType",
                            zero_state_type);
}

void LogZeroStateReceivedScore(const std::string& suffix,
                               float score,
                               float lo,
                               float hi) {
  if (suffix.empty())
    return;
  DCHECK(lo < hi);
  // Record the scaled score's floor in order to preserve its bucket.
  base::UmaHistogramExactLinear(
      "Apps.AppList.ZeroStateResults.ReceivedScore." + suffix,
      floor(100 * (score - lo) / (hi - lo)), 100);
}

void LogZeroStateResultsListMetrics(
    const std::vector<RankingItemType>& result_types,
    int launched_index) {
  // Log position of clicked items.
  if (launched_index >= 0) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "Apps.AppList.ZeroStateResultsList.LaunchedItemPositionV2",
        launched_index, 5);
  }

  // Log the number of types shown in the impression set.
  base::flat_set<RankingItemType> type_set(result_types);
  UMA_HISTOGRAM_EXACT_LINEAR(
      "Apps.AppList.ZeroStateResultsList.NumImpressionTypesV2", type_set.size(),
      5);

  // Log whether any Drive files were impressed.
  UMA_HISTOGRAM_BOOLEAN("Apps.AppList.ZeroStateResultsList.ContainsDriveFiles",
                        type_set.contains(RankingItemType::kDriveQuickAccess));

  // Log CTR metrics. Note that all clicks are captured and indicated by a
  // non-negative launch index, while an index of -1 indicates that results were
  // impressed on screen for some amount of time.
  UMA_HISTOGRAM_BOOLEAN("Apps.AppList.ZeroStateResultsList.Clicked",
                        launched_index >= 0);
}

void LogChipUsageMetrics(const AppLaunchData& launch) {
  // Filter launches that aren't from the chips.
  if (launch.launched_from !=
      ash::AppListLaunchedFrom::kLaunchedFromSuggestionChip)
    return;

  // Total usage.
  UMA_HISTOGRAM_BOOLEAN("Apps.AppList.SuggestedFiles.ChipLaunched", true);

  // Usage per chip type.
  UMA_HISTOGRAM_ENUMERATION("Apps.AppList.SuggestedFiles.LaunchType",
                            launch.ranking_item_type);

  // Launch index. At most 5 chips are shown and indices are 0-based, so the
  // maximum index is 4.
  UMA_HISTOGRAM_EXACT_LINEAR("Apps.AppList.SuggestedFiles.LaunchIndex",
                             launch.suggestion_index, 4);
}

}  // namespace app_list
