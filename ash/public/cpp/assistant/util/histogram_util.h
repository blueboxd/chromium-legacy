// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_UTIL_HISTOGRAM_UTIL_H_
#define ASH_PUBLIC_CPP_ASSISTANT_UTIL_HISTOGRAM_UTIL_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/optional.h"

namespace ash {

namespace assistant {
namespace metrics {

// Enumeration of possible results for a proactive suggestions server request.
// Note that this enum is used in UMA histograms so new values should only be
// appended to the end.
enum class ProactiveSuggestionsRequestResult {
  kError = 0,
  kSuccessWithContent = 1,
  kSuccessWithoutContent = 2,
  kMaxValue = kSuccessWithoutContent,
};

// Enumeration of possible attempt resolutions for showing a proactive
// suggestion to the user. Note that this enum is used in UMA histograms so new
// values should only be appended to the end.
enum class ProactiveSuggestionsShowAttempt {
  kSuccess = 0,
  kAbortedByDuplicateSuppression = 1,
  kMaxValue = kAbortedByDuplicateSuppression,
};

// Enumeration of possible results for having shown a proactive suggestion to
// the user. Note that this enum is used in UMA histograms so new values should
// only be appended to the end.
enum class ProactiveSuggestionsShowResult {
  kClick = 0,
  kCloseByContextChange = 1,
  kCloseByTimeout = 2,
  kCloseByUser = 3,
  kTeleport = 4,
  kMaxValue = kTeleport,
};

// Records a click on a proactive suggestions card. If provided, the opaque
// |category| of the associated content (e.g. news, shopping, etc.), the |index|
// of the card within its list, as well as the |veId| associated w/ the type of
// card are also recorded.
ASH_PUBLIC_EXPORT void RecordProactiveSuggestionsCardClick(
    base::Optional<int> category,
    base::Optional<int> index,
    base::Optional<int> veId);

// Records a |result| for a proactive suggestions server request in the
// specified content |category|. Note that |category| is an opaque int that is
// provided by the proactive suggestions server to represent the category of the
// associated content (e.g. news, shopping, etc.).
ASH_PUBLIC_EXPORT void RecordProactiveSuggestionsRequestResult(
    int category,
    ProactiveSuggestionsRequestResult result);

// Records an |attempt| to show a proactive suggestion to the user in the
// specified content |category|. Note that |category| is an opaque int that is
// provided by the proactive suggestions server to represent the category of the
// associated content (e.g. news, shopping, etc.). Also note that we record a
// different set of histograms depending on whether or not the proactive
// suggestion has been seen before so that we can measure user engagement for
// the first show attempt in comparison to on subsequent attempts for the same
// content.
ASH_PUBLIC_EXPORT void RecordProactiveSuggestionsShowAttempt(
    int category,
    ProactiveSuggestionsShowAttempt attempt,
    bool has_seen_before);

// Records a |result| from having shown a proactive suggestion to the user in
// the specified content |category|. Note that |category| is an opaque int that
// is provided by the proactive suggestions server to represent the category of
// the associated content (e.g. news, shopping, etc.). Also note that we record
// a different set of histograms depending on whether or not the proactive
// suggestion has been seen before so that we can measure user engagement for
// the first show result in comparison to subsequent results for the same
// content.
ASH_PUBLIC_EXPORT void RecordProactiveSuggestionsShowResult(
    int category,
    ProactiveSuggestionsShowResult result,
    bool has_seen_before);

// Records an impression of a proactive suggestions view. If provided, the
// opaque |category| of the associated content (e.g. news, shopping, etc.) as
// well as the |veId| associated w/ the type of view are also recorded.
ASH_PUBLIC_EXPORT void RecordProactiveSuggestionsViewImpression(
    base::Optional<int> category,
    base::Optional<int> veId);

// Records an impression of a proactive suggestions view. If provided, the
// opaque |category| of the associated content (e.g. news, shopping, etc.) as
// well as the |veId| associated w/ the type of view are also recorded.
ASH_PUBLIC_EXPORT void RecordProactiveSuggestionsViewImpression(
    base::Optional<int> category,
    base::Optional<int> veId);

}  // namespace metrics
}  // namespace assistant
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_UTIL_HISTOGRAM_UTIL_H_