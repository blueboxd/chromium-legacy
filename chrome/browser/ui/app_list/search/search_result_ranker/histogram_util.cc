// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"

#include "base/metrics/histogram_functions.h"

namespace app_list {

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

}  // namespace app_list
