// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/phone_number_regex.h"

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/sharing/click_to_call/feature.h"

namespace {

// Heuristical regex to search for phone number.
// (^|\p{Z}) makes sure the pattern begins with a new word.
// (\(?\+[0-9]+\)?) checks for optional international code in number.
// ([.\p{Z}\-\(]?[0-9][\p{Z}\-\)]?){8,} checks for at least eight occurrences of
// digits with optional separators to reduce false positives.
const char kPhoneNumberRegexPatternSimple[] =
    R"((?:^|\p{Z})((?:\(?\+[0-9]+\)?)?(?:[.\p{Z}\-(]?[0-9][\p{Z}\-)]?){8,}))";

void PrecompilePhoneNumberRegexes() {
  SCOPED_UMA_HISTOGRAM_TIMER("Sharing.ClickToCallPhoneNumberPrecompileTime");
  static const char kExampleInput[] = "+01(2)34-5678 9012";
  std::string parsed;
  for (auto variant : {PhoneNumberRegexVariant::kSimple}) {
    // Run RE2::PartialMatch over some example input to speed up future queries.
    re2::RE2::PartialMatch(kExampleInput, GetPhoneNumberRegex(variant),
                           &parsed);
  }
}

}  // namespace

const re2::RE2& GetPhoneNumberRegex(PhoneNumberRegexVariant variant) {
  static const re2::LazyRE2 kRegexSimple = {kPhoneNumberRegexPatternSimple};

  switch (variant) {
    case PhoneNumberRegexVariant::kSimple:
      return *kRegexSimple;
  }
}

void PrecompilePhoneNumberRegexesAsync() {
  if (!base::FeatureList::IsEnabled(kClickToCallUI))
    return;
  constexpr auto kParseDelay = base::TimeDelta::FromSeconds(15);
  base::PostDelayedTask(FROM_HERE,
                        {base::ThreadPool(), base::TaskPriority::BEST_EFFORT,
                         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                        base::BindOnce(&PrecompilePhoneNumberRegexes),
                        kParseDelay);
}
