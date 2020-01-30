// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/quick_answers_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

namespace chromeos {
namespace quick_answers {

namespace {
const char kQuickAnswerClick[] = "QuickAnswers.Click";
const char kQuickAnswerResult[] = "QuickAnswers.Result";
const char kQuickAnswerLoadingStatus[] = "QuickAnswers.Loading.Status";
const char kQuickAnswerLoadingDuration[] = "QuickAnswers.Loading.Duration";
const char kDurationSuffix[] = ".Duration";

std::string ResultTypeToString(ResultType result_type) {
  switch (result_type) {
    case ResultType::kNoResult:
      return ".NoResult";
    case ResultType::kDefinitionResult:
      return ".Definition";
    case ResultType::kTranslationResult:
      return ".Translation";
    case ResultType::kUnitConversionResult:
      return ".UnitConversion";
    default:
      NOTREACHED() << "Invalid ResultType.";
      return ".Unknown";
  }
}

void RecordTypeAndDuration(const std::string& prefix,
                           ResultType result_type,
                           const base::TimeDelta duration,
                           bool is_medium_bucketization) {
  // Record by result type.
  base::UmaHistogramSparse(prefix, static_cast<int>(result_type));

  const std::string duration_histogram = prefix + kDurationSuffix;
  const std::string result_type_histogram_name =
      duration_histogram + ResultTypeToString(result_type);
  // Record sliced by duration and result type.
  if (is_medium_bucketization) {
    base::UmaHistogramMediumTimes(duration_histogram, duration);
    base::UmaHistogramMediumTimes(result_type_histogram_name.c_str(), duration);
  } else {
    base::UmaHistogramTimes(duration_histogram, duration);
    base::UmaHistogramTimes(result_type_histogram_name.c_str(), duration);
  }
}

}  // namespace

void RecordResult(ResultType result_type, const base::TimeDelta duration) {
  RecordTypeAndDuration(kQuickAnswerResult, result_type, duration,
                        /*is_medium_bucketization=*/false);
}

void RecordLoadingStatus(LoadStatus status, const base::TimeDelta duration) {
  base::UmaHistogramEnumeration(kQuickAnswerLoadingStatus, status);
  base::UmaHistogramTimes(kQuickAnswerLoadingDuration, duration);
}

void RecordClick(ResultType result_type, const base::TimeDelta duration) {
  RecordTypeAndDuration(kQuickAnswerClick, result_type, duration,
                        /*is_medium_bucketization=*/true);
}

}  // namespace quick_answers
}  // namespace chromeos
