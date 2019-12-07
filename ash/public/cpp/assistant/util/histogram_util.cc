// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/util/histogram_util.h"

#include "ash/public/cpp/assistant/proactive_suggestions.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"

namespace ash {
namespace assistant {
namespace metrics {

void RecordProactiveSuggestionsCardClick(base::Optional<int> category,
                                         base::Optional<int> index,
                                         base::Optional<int> veId) {
  constexpr char kCardClickHistogram[] =
      "Assistant.ProactiveSuggestions.CardClick";

  // Record the top level histogram for every click event.
  base::UmaHistogramBoolean(kCardClickHistogram, true);

  // If possible, record by |category| so we can slice during analysis.
  if (category.has_value()) {
    base::UmaHistogramSparse(
        base::StringPrintf("%s.ByCategory", kCardClickHistogram),
        category.value());
  }

  // If possible, record by |index| so we can slice during analysis.
  if (index.has_value()) {
    base::UmaHistogramSparse(
        base::StringPrintf("%s.ByIndex", kCardClickHistogram), index.value());
  }

  // If possible, record by |veId| so we can slice during analysis.
  if (veId.has_value()) {
    base::UmaHistogramSparse(
        base::StringPrintf("%s.ByVeId", kCardClickHistogram), veId.value());
  }
}

void RecordProactiveSuggestionsRequestResult(
    int category,
    ProactiveSuggestionsRequestResult result) {
  constexpr char kRequestResultHistogram[] =
      "Assistant.ProactiveSuggestions.RequestResult";

  // We record an aggregate histogram for easily reporting cumulative request
  // results across all content categories.
  base::UmaHistogramEnumeration(kRequestResultHistogram, result);

  // We record sparse histograms for easily comparing request results between
  // content categories.
  switch (result) {
    case ProactiveSuggestionsRequestResult::kError:
      base::UmaHistogramSparse(
          base::StringPrintf("%s.Error", kRequestResultHistogram), category);
      break;
    case ProactiveSuggestionsRequestResult::kSuccessWithContent:
      base::UmaHistogramSparse(
          base::StringPrintf("%s.SuccessWithContent", kRequestResultHistogram),
          category);
      break;
    case ProactiveSuggestionsRequestResult::kSuccessWithoutContent:
      base::UmaHistogramSparse(base::StringPrintf("%s.SuccessWithoutContent",
                                                  kRequestResultHistogram),
                               category);
      break;
  }
}

void RecordProactiveSuggestionsShowAttempt(
    int category,
    ProactiveSuggestionsShowAttempt attempt,
    bool has_seen_before) {
  constexpr char kFirstShowAttemptHistogram[] =
      "Assistant.ProactiveSuggestions.FirstShowAttempt";
  constexpr char kReshowAttemptHistogram[] =
      "Assistant.ProactiveSuggestions.ReshowAttempt";

  // We use a different set of histograms depending on whether or not the set
  // of proactive suggestions has already been seen. This allows us to measure
  // user engagement the first time the entry point is presented in comparison
  // to follow up presentations of the same content.
  const std::string histogram =
      has_seen_before ? kReshowAttemptHistogram : kFirstShowAttemptHistogram;

  // We record an aggregate histogram for easily reporting cumulative show
  // attempts across all content categories.
  base::UmaHistogramEnumeration(histogram, attempt);

  // We record sparse histograms for easily comparing show attempts between
  // content categories.
  switch (attempt) {
    case ProactiveSuggestionsShowAttempt::kSuccess:
      base::UmaHistogramSparse(
          base::StringPrintf("%s.Success.ByCategory", histogram.c_str()),
          category);
      break;
    case ProactiveSuggestionsShowAttempt::kAbortedByDuplicateSuppression:
      base::UmaHistogramSparse(
          base::StringPrintf("%s.AbortedByDuplicateSuppression.ByCategory",
                             histogram.c_str()),
          category);
      break;
  }
}

void RecordProactiveSuggestionsShowResult(int category,
                                          ProactiveSuggestionsShowResult result,
                                          bool has_seen_before) {
  constexpr char kFirstShowResultHistogram[] =
      "Assistant.ProactiveSuggestions.FirstShowResult";
  constexpr char kReshowResultHistogram[] =
      "Assistant.ProactiveSuggestions.ReshowResult";

  // We use a different set of histograms depending on whether or not the set
  // of proactive suggestions has already been seen. This allows us to measure
  // user engagement the first time the entry point is presented in comparison
  // to follow up presentations of the same content.
  const std::string histogram =
      has_seen_before ? kReshowResultHistogram : kFirstShowResultHistogram;

  // We record an aggregate histogram for easily reporting cumulative show
  // results across all content categories.
  base::UmaHistogramEnumeration(histogram, result);

  // We record sparse histograms for easily comparing show results between
  // content categories.
  switch (result) {
    case ProactiveSuggestionsShowResult::kClick:
      base::UmaHistogramSparse(
          base::StringPrintf("%s.Click.ByCategory", histogram.c_str()),
          category);
      break;
    case ProactiveSuggestionsShowResult::kCloseByContextChange:
      base::UmaHistogramSparse(
          base::StringPrintf("%s.CloseByContextChange.ByCategory",
                             histogram.c_str()),
          category);
      break;
    case ProactiveSuggestionsShowResult::kCloseByTimeout:
      base::UmaHistogramSparse(
          base::StringPrintf("%s.CloseByTimeout.ByCategory", histogram.c_str()),
          category);
      break;
    case ProactiveSuggestionsShowResult::kCloseByUser:
      base::UmaHistogramSparse(
          base::StringPrintf("%s.CloseByUser.ByCategory", histogram.c_str()),
          category);
      break;
    case ProactiveSuggestionsShowResult::kTeleport:
      base::UmaHistogramSparse(
          base::StringPrintf("%s.Teleport.ByCategory", histogram.c_str()),
          category);
      break;
  }
}

void RecordProactiveSuggestionsViewImpression(base::Optional<int> category,
                                              base::Optional<int> veId) {
  constexpr char kViewImpressionHistogram[] =
      "Assistant.ProactiveSuggestions.ViewImpression";

  // Record the top level histogram for every impression event.
  base::UmaHistogramBoolean(kViewImpressionHistogram, true);

  // If possible, record by |category| so we can slice during analysis.
  if (category.has_value()) {
    base::UmaHistogramSparse(
        base::StringPrintf("%s.ByCategory", kViewImpressionHistogram),
        category.value());
  }

  // If possible, record by |veId| so we can slice during analysis.
  if (veId.has_value()) {
    base::UmaHistogramSparse(
        base::StringPrintf("%s.ByVeId", kViewImpressionHistogram),
        veId.value());
  }
}

}  // namespace metrics
}  // namespace assistant
}  // namespace ash
