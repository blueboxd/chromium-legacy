// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cookie_store/cookie_store_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace blink {

// This enum describes the MatchType value specified by the user.
enum class MatchTypeOption {
  // Do not change the meaning or ordering of these values because they are
  // being recorded in a UMA metric.
  kUnspecified = 0,
  kEquals = 1,
  kStartsWith = 2,
  kMaxValue = kStartsWith
};

void RecordMatchType(const String& matchType) {
  MatchTypeOption uma_match_type;
  if (matchType.IsEmpty()) {
    uma_match_type = MatchTypeOption::kUnspecified;
  } else if (matchType == "starts-with") {
    uma_match_type = MatchTypeOption::kStartsWith;
  } else {
    uma_match_type = MatchTypeOption::kEquals;
  }
  UMA_HISTOGRAM_ENUMERATION("Blink.CookieStore.MatchType", uma_match_type);
}

}  // namespace blink
