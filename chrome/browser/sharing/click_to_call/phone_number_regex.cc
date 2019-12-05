// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/phone_number_regex.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

// Heuristical regex to search for phone number.
// (^|\p{Z}) makes sure the pattern begins with a new word.
// (\(?\+[0-9]+\)?) checks for optional international code in number.
// ([.\p{Z}\-\(]?[0-9][\p{Z}\-\)]?){8,} checks for at least eight occurrences of
// digits with optional separators to reduce false positives.
const char kPhoneNumberRegexPatternSimple[] =
    R"((?:^|\p{Z})((?:\(?\+[0-9]+\)?)?(?:[.\p{Z}\-(]?[0-9][\p{Z}\-)]?){8,}))";

// Regex based on the low confidence pattern used in TextClassifier on Android.
// The original low confidence regex has been modified to support some more
// patterns that are common in our user base.
// https://developer.android.com/reference/android/view/textclassifier/TextClassifier
// https://android.googlesource.com/platform/external/libtextclassifier/+/refs/heads/master/models/
const char kPhoneNumberRegexPatternLowConfidenceModified[] =
    R"((?:^|[^\/+.\d\p{Pd}:\p{Z})]|(?:^|[^\/+.\d\p{Pd})])[:\p{Z})])(?:^|(?sm:\)"
    R"(b)|[\p{Z}:.])((?:(?:(?:\(\d{4}\)[\p{Pd}\p{Z}]\d{2}(?:\p{Pd}\d{2}){2}[\p)"
    R"({Pd}\p{Z}]|\+[\p{Pd}\p{Z}]\d(?:\d{2}[\p{Pd}\p{Z}]\(\d{2}\)[\p{Pd}\p{Z}])"
    R"(\d|[\p{Pd}\p{Z}]\(\d{4}\)[\p{Pd}\p{Z}])|\d{3}[\p{Pd}\p{Z}]\(\d{2}\)[\p{)"
    R"(Pd}\p{Z}]\d)(?:\d{2}\p{Pd}){2}|(?:\d{2}(?:[\p{Pd}\p{Z}](?:\d{2}(?:[\p{P)"
    R"(d}\p{Z}]\d{2}(?:[\p{Pd}\p{Z}]\d{4}[\p{Pd}\p{Z}]|\d{2}\p{Pd}|\d\p{Pd}\d))"
    R"(|\d{2}\p{Pd}?\d)|\(\d{2}\)[\p{Pd}\p{Z}]\d)\d{2}[\p{Pd}\p{Z}]|(?:\p{Pd}()"
    R"(?:\d{2}[\p{Pd}\p{Z}]){2}\d{4}\p{Pd}|\d\p{Pd}\d|\d{2}[\p{Pd}\p{Z}]|\d[\p)"
    R"({Pd}\p{Z}])\d{2}[\p{Pd}\p{Z}])|\+[\p{Pd}\p{Z}]\d{2}[\p{Pd}\p{Z}]\(\d{2})"
    R"(\)[\p{Pd}\p{Z}]\d{3}[\p{Pd}\p{Z}])\d{2}[\p{Pd}\p{Z}]|\+(?:\d(?:\d(?:[\p)"
    R"({Pd}\p{Z}](?:\(\d(?:(?:\)(?:[\p{Pd}\p{Z}]\d{4}\p{Pd}\d{2,3}|\d{4}\p{Pd})"
    R"(\d{3})|(?:\)(?:[\p{Pd}\p{Z}]\d{3}\p{Pd}\d|\d[\p{Pd}\p{Z}]\d{2}[\p{Pd}\p)"
    R"({Z}]|\d{3}\p{Pd}\d)|\d{3}\)[\p{Pd}\p{Z}]\d)\d{2})[\p{Pd}\p{Z}]\d{2}[\p{)"
    R"(Pd}\p{Z}]|\d(?:\d(?:\d\)[\p{Pd}\p{Z}]\d{2}(?:\p{Pd}\d{2}\p{Pd}|\d{2})|\)"
    R"()[\p{Pd}\p{Z}]\d{5})|\)[\p{Pd}\p{Z}]\d{3}(?:[\p{Pd}\p{Z}]\d{2}[\p{Pd}\p)"
    R"({Z}]|\p{Pd}\d{2}\p{Pd})))|\d{3}(?:[\p{Pd}\p{Z}]\d{5}|\d\p{Pd}\d{3}[\p{P)"
    R"(d}\p{Z}]|\d{2}[\p{Pd}\p{Z}]\d{3}))|(?:\d[\p{Pd}\p{Z}]\(\d{2}\)[\p{Pd}\p)"
    R"({Z}]\d{3}[\p{Pd}\p{Z}]|[\p{Pd}\p{Z}]\d(?:[\p{Pd}\p{Z}]\d{2}[\p{Pd}\p{Z})"
    R"(]|\d{2}(?:\d\p{Pd}?|[\p{Pd}\p{Z}]))\d{2}[\p{Pd}\p{Z}])\d{2}[\p{Pd}\p{Z})"
    R"(]|\d(?:(?:[\p{Pd}\p{Z}]\(\d{2}\)[\p{Pd}\p{Z}]\d{3}|\d{4,5})\p{Pd}\d{2}\)"
    R"(p{Pd}|[\p{Pd}\p{Z}]\(\d{3}\)[\p{Pd}\p{Z}]\d{3}[\p{Pd}\p{Z}]?\d{2}|\d{8})"
    R"([\p{Pd}\p{Z}]\p{Pd}[\p{Pd}\p{Z}]))|(?:\d(?:[\p{Pd}\p{Z}](?:\(\d(?:\d(?:)"
    R"(\d\)[\p{Pd}\p{Z}]\d{3}(?:\d\p{Pd}|[\p{Pd}\p{Z}])|\)(?:[\p{Pd}\p{Z}]\d{4)"
    R"(}(?:\d?\p{Pd}|[\p{Pd}\p{Z}])|\d{4}\p{Pd}))|(?:\)\d(?:\d\p{Pd}\d{2}|\d[\)"
    R"(p{Pd}\p{Z}]\d|\d{2}[\p{Pd}\p{Z}]|[\p{Pd}\p{Z}]\d{2})|\d{2}\)\d)\d{2})|\)"
    R"(d{2}(?:[\p{Pd}\p{Z}]\d{4}[\p{Pd}\p{Z}]|\p{Pd}\d{4}))|(?:\(\d\)\d{2}\p{P)"
    R"(d}\d{4}|\d[\p{Pd}\p{Z}]\d{4,5})[\p{Pd}\p{Z}]|\p{Pd}\d{2}\p{Pd}\d{3}\p{P)"
    R"(d}\d)|(?:\d\p{Pd}\d{3}\p{Pd}\d|[\p{Pd}\p{Z}]\d{3}\p{Pd})\d{3}\p{Pd}|(?:)"
    R"(\d\.|[\p{Pd}\p{Z}])\d{3}\.\d{3}\.|[\p{Pd}\p{Z}](?:\d{3}[\p{Pd}\p{Z}]){2)"
    R"(}|\.\d{3}(?:\.\d{3}\.|\p{Pd}\d{3}\p{Pd}))\d{2}|[\p{Pd}\p{Z}]\(\d{3}(?:\)"
    R"()(?:[\p{Pd}\p{Z}]\d{3}(?:\p{Pd}\d{2}\p{Pd}|[\p{Pd}\p{Z}]?\d{2})|\d{3}\p)"
    R"({Pd}?\d{2})|\d(?:(?:\d\)[\p{Pd}\p{Z}]|\)[\p{Pd}\p{Z}]\d)\d\p{Pd}\d{2}\p)"
    R"({Pd}|\)[\p{Pd}\p{Z}]\d{3}\p{Pd}?\d)))|[\p{Pd}\p{Z}]\d(?:[\p{Pd}\p{Z}]\()"
    R"(\d{3}\)[\p{Pd}\p{Z}]\d{3}(?:[\p{Pd}\p{Z}]\d{2}[\p{Pd}\p{Z}]|\d{2})|\d[\)"
    R"(p{Pd}\p{Z}]\(\d{3}\)[\p{Pd}\p{Z}]\d{5}))|(?:\((?:(?:\+\d{2}(?:\d{2}\)[\)"
    R"(p{Pd}\p{Z}]\d{3}[\p{Pd}\p{Z}]|\)[\p{Pd}\p{Z}]\d{2}\p{Pd}?)\d|\d\)[\p{Pd)"
    R"(}\p{Z}]\(\d{3}\)[\p{Pd}\p{Z}])\d{3}|\d{2}(?:\)(?:[\p{Pd}\p{Z}](?:\(\d{4)"
    R"(}\)[\p{Pd}\p{Z}]\d{3}|\d{3}(?:\p{Pd}\d{3}\p{Pd}|\d{2}\p{Pd}|\d\p{Pd}|\d)"
    R"([\p{Pd}\p{Z}]))|\d{4}\p{Pd})|\d(?:\)(?:[\p{Pd}\p{Z}]\d{3}(?:[\p{Pd}\p{Z)"
    R"(}]\d{3}[\p{Pd}\p{Z}]|\p{Pd}\d{3}\p{Pd}|\d\p{Pd})|\d{3}\p{Pd})|\d\)[\p{P)"
    R"(d}\p{Z}]\d{3}\p{Pd}))|\+\d\)[\p{Pd}\p{Z}]\d{3}(?:\.\d{3}\.|\p{Pd}\d{3}\)"
    R"(p{Pd}))|\+[\p{Pd}\p{Z}]\d{2}[\p{Pd}\p{Z}]\(\d{2}(?:\)[\p{Pd}\p{Z}]\d{4})"
    R"(\p{Pd}|\d\)[\p{Pd}\p{Z}]\d{3}[\p{Pd}\p{Z}])|(?:\((?:\+\d)?\d\)|\d)[\p{P)"
    R"(d}\p{Z}](?:\d{3}\p{Pd}){2}|\d(?:\d(?:(?:\d[\p{Pd}\p{Z}]\(\d{3}\)[\p{Pd})"
    R"(\p{Z}]\d|\d{2}\/|\p{Pd}\d{2})\d{2}|\d(?:[\p{Pd}\p{Z}]\d{4}[\p{Pd}\p{Z}])"
    R"(|\d[\p{Pd}\p{Z}]\d{3}[\p{Pd}\p{Z}]|\.\d{3}\.|\d{2}\p{Pd}\d{2}|\d{3}\p{P)"
    R"(d})|[\p{Pd}\p{Z}]\d{4}[\p{Pd}\p{Z}])|\.\d{3}\.\d{3}\.))\d{2}|\((?:\+\d{)"
    R"(2}\)(?:\d{2}\.){4}|\d{2}(?:\d{2}\)[\p{Pd}\p{Z}]\d{3}(?:[\p{Pd}\p{Z}](?:)"
    R"(\d{2}[\p{Pd}\p{Z}]){2}|\d{2})|\)[\p{Pd}\p{Z}](?:\(\d{2}(?:\)[\p{Pd}\p{Z)"
    R"(}]\d|\d\)[\p{Pd}\p{Z}])\d{3}\p{Pd}?\d{2}|\d{3}(?:(?:\p{Pd}\d{2}){2}[\p{)"
    R"(Pd}\p{Z}]\d|[\p{Pd}\p{Z}](?:\d{2}[\p{Pd}\p{Z}]){2}|\d{3}))))|\d(?:\d(?:)"
    R"(\d(?:[\p{Pd}\p{Z}]\d{4}\p{Pd}(?:\d{3}[\p{Pd}\p{Z}]){2}|\d\p{Pd}\d{3}[\p)"
    R"({Pd}\p{Z}](?:\d{2}[\p{Pd}\p{Z}]){2}|\d{3}[\p{Pd}\p{Z}]\d{2}[\p{Pd}\p{Z})"
    R"(]|\p{Pd}\d{5}|(?:\d{2}[\p{Pd}\p{Z}]){2}|\d{6}|\p{Pd}\d{2}\p{Pd})|[\p{Pd)"
    R"(}\p{Z}](?:\(\d{2}(?:\d\)[\p{Pd}\p{Z}]|\)[\p{Pd}\p{Z}]\d)\d{3}[\p{Pd}\p{)"
    R"(Z}]?\d{2}|(?:\d{2}[\p{Pd}\p{Z}]){3})|\.(?:\d{2}\.){3})|\p{Pd}\d{3}\p{Pd)"
    R"(}\d{5})|(?:\+[\d\p{Pd}\p{Z}]\d[\p{Pd}\p{Z}]\(\d{3}\)[\p{Pd}\p{Z}]|\d[\p)"
    R"({Pd}\p{Z}]\(\d{3}\)[\p{Pd}\p{Z}]|\d\p{Pd}\d{3}\p{Pd})\d{3}\p{Pd}\d{2}\p)"
    R"({Pd}?|(?:\+\d{2}[\p{Pd}\p{Z}]\d{2,3}[\p{Pd}\p{Z}]|\d{3}[\d\p{Pd}\p{Z}]))"
    R"(\d{3}[\p{Pd}\p{Z}]\d{2}[\p{Pd}\p{Z}]?)\d{2}|\+\d(?:\d(?:[\p{Pd}\p{Z}](?)"
    R"(:(?:\(\d(?:\)\d{4}\p{Pd}(?:\d{2}[\p{Pd}\p{Z}]){3}|\d{4}\)[\p{Pd}\p{Z}]\)"
    R"(d{5}[\p{Pd}\p{Z}]|\d\)[\p{Pd}\p{Z}]\d{8})|\(\d(?:\)\d{4}\p{Pd}\d{2}(?:[)"
    R"(\p{Pd}\p{Z}]\d)?\d[\p{Pd}\p{Z}]|\d(?:\d{3}\)[\p{Pd}\p{Z}]|\)[\p{Pd}\p{Z)"
    R"(}]\d{3})\d{3})\d)\d|\d{2}(?:\d{2}(?:[\p{Pd}\p{Z}]?\d{4}[\p{Pd}\p{Z}]\d{)"
    R"(3}|[\p{Pd}\p{Z}]\d{5}(?:\d[\p{Pd}\p{Z}]|[\p{Pd}\p{Z}]\d)|\d{6}[\p{Pd}\p)"
    R"({Z}])|(?:(?:[\d\p{Pd}\p{Z}](?:\d{2}[\p{Pd}\p{Z}]){2}|[\p{Pd}\p{Z}]\d{5})"
    R"()\d{2}[\p{Pd}\p{Z}]|[\p{Pd}\p{Z}]\d{6})\d)\d|\d{2}(?:(?:(?:[\d\p{Pd}\p{)"
    R"(Z}](?:\d{2}[\p{Pd}\p{Z}]){2}|[\p{Pd}\p{Z}]\d{5})\d{2}[\p{Pd}\p{Z}]|(?:[)"
    R"(\d\p{Pd}\p{Z}](?:\d{2}[\p{Pd}\p{Z}]){2}|[\p{Pd}\p{Z}]\d{5})\d)\d|\d{2}()"
    R"(?:[\p{Pd}\p{Z}]?\d{4}[\p{Pd}\p{Z}](?:\d{2}|\d)|[\p{Pd}\p{Z}]\d{4}(?:\d[)"
    R"(\p{Pd}\p{Z}]\d|\d{2}|\d)?|\d{4,6})))|\d{3}(?:(?:[\p{Pd}\p{Z}]\d{2}(?:[\)"
    R"(p{Pd}\p{Z}](?:\d{2}[\p{Pd}\p{Z}]){2}\d|\d{4}(?:\d[\p{Pd}\p{Z}]|[\p{Pd}\)"
    R"(p{Z}]\d))|\d{3}(?:[\p{Pd}\p{Z}](?:\d{2}[\p{Pd}\p{Z}]){2}|\d{3}[\p{Pd}\p)"
    R"({Z}]\d|[\p{Pd}\p{Z}]\d{3}|\d{3}))\d|[\p{Pd}\p{Z}]\d{2}(?:(?:[\p{Pd}\p{Z)"
    R"(}]\d{2}){2}(?:[\p{Pd}\p{Z}]\d)?|\d{4}(?:[\p{Pd}\p{Z}]?\d)?)|\d{3}(?:(?:)"
    R"([\p{Pd}\p{Z}]\d{2}){2}|\d{2}(?:\d[\p{Pd}\p{Z}]\d|\d)?)))|[\p{Pd}\p{Z}]\)"
    R"((\d{3}\)[\p{Pd}\p{Z}]\d{3}\p{Pd}\d{3,4})|\(\d{2}(?:\d(?:\)(?:(?:[\p{Pd})"
    R"(\p{Z}]\d{3}(?:\p{Pd}\d{2}(?:\d{2}[\p{Pd}\p{Z}]\d{5}|\p{Pd}\d{2}[\p{Pd}\)"
    R"(p{Z}])|[\p{Pd}\p{Z}](?:\d{2}[\p{Pd}\p{Z}]){2}\d|[\p{Pd}\p{Z}]\d{3}|\d{4)"
    R"(})|\d{7}[\p{Pd}\p{Z}]|\d{6})\d|[\p{Pd}\p{Z}]\d{3}(?:\p{Pd}\d{2}(?:\d{2})"
    R"((?:[\p{Pd}\p{Z}]\d{3,4})?|\p{Pd}\d{2})|[\p{Pd}\p{Z}](?:\d{2}[\p{Pd}\p{Z)"
    R"(}]){2}\d|(?:[\p{Pd}\p{Z}]\d{2}){2}|\d{4}))|\d\)[\p{Pd}\p{Z}]\d{2}(?:(?:)"
    R"(\p{Pd}\d{2}){2}(?:[\p{Pd}\p{Z}]\d)?|\d(?:[\p{Pd}\p{Z}]\d{2}){2}))|\)[\p)"
    R"({Pd}\p{Z}]\d{3}(?:\p{Pd}\d{2}\p{Pd}|[\p{Pd}\p{Z}]\d{2}[\p{Pd}\p{Z}])\d{)"
    R"(2})|\d(?:[\p{Pd}\p{Z}]\(\d{3}(?:\d\)[\p{Pd}\p{Z}](?:\d{2}\p{Pd}){2}|\)()"
    R"(?:[\p{Pd}\p{Z}]\d{3}[\p{Pd}\p{Z}]\d{2}[\p{Pd}\p{Z}]?|[\p{Pd}\p{Z}]?\d{5)"
    R"(}))\d{2}|\d(?:\d(?:(?:(?:\p{Pd}\d{3}\p{Pd}\d{4}[\p{Pd}\p{Z}](?:\d{2}|\d)"
    R"()|\d\p{Pd}(?:\d{2}[\p{Pd}\p{Z}]){3}|\d{3}[\p{Pd}\p{Z}]\d{3}|\d\p{Pd}\d{)"
    R"(5})\d|\d\p{Pd}(?:\d{2}[\p{Pd}\p{Z}]){3}|\d{3}[\p{Pd}\p{Z}]\d{3}|\d\p{Pd)"
    R"(}\d{5})\d|[\p{Pd}\p{Z}]\d{4}(?:(?:\p{Pd}\d{3}[\p{Pd}\p{Z}])?\d{4}|\d{1,)"
    R"(3})|\d(?:(?:\p{Pd}\d{2}(?:\d[\p{Pd}\p{Z}]\d{2}[\p{Pd}\p{Z}]\d|[\p{Pd}\p)"
    R"({Z}]\d{2}[\p{Pd}\p{Z}]\d|\d[\p{Pd}\p{Z}]\d|\d)|(?:[\p{Pd}\p{Z}]\d{2}|\d)"
    R"([\p{Pd}\p{Z}]\d|\d\/)\d{5})\d|[\p{Pd}\p{Z}]\d{4,7}|\d(?:[\p{Pd}\p{Z}]\d)"
    R"({4,6}|\/\d{4,5}))|\p{Pd}\d{3}(?:\p{Pd}\d{3,4}|\d))|[\p{Pd}\p{Z}]\d{2}(?)"
    R"(:\d{2}(?:\p{Pd}?\d)?\d|[\p{Pd}\p{Z}]\d{2}[\p{Pd}\p{Z}])\d{2})))(?i:[\,\)"
    R"(;]?(?:\p{Z})?(?:[x\#\~]|ext[\.\:\=]?)(?:\p{Z})?\d{1,6})?)(?:$|(?sm:\b)|)"
    R"(\p{Z})(?:$|\p{Z}(?:$|[^\p{Z}\d\/+=#\p{Pd}])|\.(?:$|\D)|[^.\p{Z}\d\/+=#\)"
    R"(p{Pd}]))";

void PrecompilePhoneNumberRegexes() {
  SCOPED_UMA_HISTOGRAM_TIMER("Sharing.ClickToCallPhoneNumberPrecompileTime");
  static const char kExampleInput[] = "+01(2)34-5678 9012";
  std::vector<PhoneNumberRegexVariant> variants = {
      PhoneNumberRegexVariant::kSimple};

  // Only precompile the low confidence regex when the flag is enabled.
  if (base::FeatureList::IsEnabled(kClickToCallDetectionV2))
    variants.push_back(PhoneNumberRegexVariant::kLowConfidenceModified);

  std::string parsed;
  for (auto variant : variants) {
    // Run RE2::PartialMatch over some example input to speed up future queries.
    re2::RE2::PartialMatch(kExampleInput, GetPhoneNumberRegex(variant),
                           &parsed);
  }
}

}  // namespace

const re2::RE2& GetPhoneNumberRegex(PhoneNumberRegexVariant variant) {
  static const re2::LazyRE2 kRegexSimple = {kPhoneNumberRegexPatternSimple};
  static const re2::LazyRE2 kRegexLowConfidenceModified = {
      kPhoneNumberRegexPatternLowConfidenceModified};

  switch (variant) {
    case PhoneNumberRegexVariant::kSimple:
      return *kRegexSimple;
    case PhoneNumberRegexVariant::kLowConfidenceModified:
      DCHECK(base::FeatureList::IsEnabled(kClickToCallDetectionV2));
      return *kRegexLowConfidenceModified;
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
