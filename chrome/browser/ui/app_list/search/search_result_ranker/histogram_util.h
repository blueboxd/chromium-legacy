// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_HISTOGRAM_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_HISTOGRAM_UTIL_H_

namespace app_list {

// Represents various model configuration errors that could occur. These values
// persist to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class ConfigurationError {
  kHashMismatch = 0,
  kInvalidParameter = 1,
  kInvalidPredictor = 2,
  kFakePredictorUsed = 3,
  kMaxValue = kFakePredictorUsed,
};

// Represents errors in saving or loading models. These values persist to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class SerializationError {
  kModelReadError = 0,
  kModelWriteError = 1,
  kFromProtoError = 2,
  kToProtoError = 3,
  kPredictorMissingError = 4,
  kTargetsMissingError = 5,
  kConditionsMissingError = 6,
  kFakePredictorLoadingError = 7,
  kFrecencyPredictorLoadingError = 8,
  kHourBinnedPredictorLoadingError = 9,
  kMaxValue = kHourBinnedPredictorLoadingError,
};

// Represents errors where a RecurrenceRanker is used in a way not supported by
// the configured predictor. These values persist to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class UsageError {
  kInvalidTrainCall = 0,
  kInvalidRankCall = 1,
  kMaxValue = kInvalidRankCall,
};

// Log a configuration error to UMA.
void LogConfigurationError(ConfigurationError error);

// Log a serialisation error to UMA.
void LogSerializationError(SerializationError error);

// Log a usage error to UMA.
void LogUsageError(UsageError error);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_HISTOGRAM_UTIL_H_
