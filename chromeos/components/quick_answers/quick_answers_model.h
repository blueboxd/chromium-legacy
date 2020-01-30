// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_MODEL_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_MODEL_H_

#include <string>

namespace chromeos {
namespace quick_answers {

// The status of loading quick answers.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Note: Enums labels are at |QuickAnswersLoadStatus|.
enum class LoadStatus {
  kSuccess = 0,
  kNetworkError = 1,
  kNoResult = 2,
  kMaxValue = kNoResult,
};

// The type of the result. Valid values are map to the search result types.
// Please see go/1ns-doc for more detail.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Note: Enums labels are at |QuickAnswersResultType|.
enum class ResultType {
  kNoResult = 0,
  kDefinitionResult = 5493,
  kTranslationResult = 6613,
  kUnitConversionResult = 13668,
};

// Structure to describe a quick answer.
struct QuickAnswer {
  ResultType result_type;
  std::string primary_answer;
  std::string secondary_answer;
};

// Structure to describe an quick answer request including selected content and
// context.
struct QuickAnswersRequest {
  // The selected Text.
  std::string selected_text;

  // TODO(llin): Add context and other targeted objects (e.g: images, links,
  // etc).
};

}  // namespace quick_answers
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_MODEL_H_
