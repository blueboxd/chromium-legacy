// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_metrics.h"

#include <algorithm>
#include <cctype>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/click_to_call/phone_number_regex.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

// The returned values must match the values of the SharingClickToCallEntryPoint
// suffixes defined in histograms.xml.
const char* ClickToCallEntryPointToSuffix(
    SharingClickToCallEntryPoint entry_point) {
  switch (entry_point) {
    case SharingClickToCallEntryPoint::kLeftClickLink:
      return "LeftClickLink";
    case SharingClickToCallEntryPoint::kRightClickLink:
      return "RightClickLink";
    case SharingClickToCallEntryPoint::kRightClickSelection:
      return "RightClickSelection";
  }
}

// The returned values must match the values of the PhoneNumberRegexVariant
// suffixes defined in histograms.xml.
const char* PhoneNumberRegexVariantToSuffix(PhoneNumberRegexVariant variant) {
  switch (variant) {
    case PhoneNumberRegexVariant::kSimple:
      return "Simple";
    case PhoneNumberRegexVariant::kLowConfidenceModified:
      return "LowConfidenceModified";
  }
}

// The returned values must match the values of the
// SharingClickToCallSendToDevice suffixes defined in histograms.xml.
const char* SendToDeviceToSuffix(bool send_to_device) {
  return send_to_device ? "Sending" : "Showing";
}

bool RegexMatches(const std::string& selection_text,
                  bool sent_to_device,
                  PhoneNumberRegexVariant variant) {
  auto used_variant = base::FeatureList::IsEnabled(kClickToCallDetectionV2)
                          ? PhoneNumberRegexVariant::kLowConfidenceModified
                          : PhoneNumberRegexVariant::kSimple;
  // Do not use ExtractPhoneNumber as it would log the parse delay again.
  if (variant == used_variant || sent_to_device)
    return re2::RE2::PartialMatch(selection_text, GetPhoneNumberRegex(variant));

  return ExtractPhoneNumber(selection_text, variant).has_value();
}

void LogPhoneNumberDetectionMetricsOnThreadPool(
    const std::string& selection_text,
    bool sent_to_device) {
  bool simple_regex_matches = RegexMatches(selection_text, sent_to_device,
                                           PhoneNumberRegexVariant::kSimple);

  for (auto variant : {PhoneNumberRegexVariant::kLowConfidenceModified}) {
    DCHECK_NE(PhoneNumberRegexVariant::kSimple, variant);
    bool matches = RegexMatches(selection_text, sent_to_device, variant);
    PhoneNumberRegexVariantResult result;
    if (simple_regex_matches) {
      result = matches ? PhoneNumberRegexVariantResult::kBothMatch
                       : PhoneNumberRegexVariantResult::kOnlySimpleMatches;
    } else {
      result = matches ? PhoneNumberRegexVariantResult::kOnlyVariantMatches
                       : PhoneNumberRegexVariantResult::kNoneMatch;
    }

    base::UmaHistogramEnumeration(
        base::StrCat({"Sharing.ClickToCallPhoneNumberRegexVariantResult.",
                      PhoneNumberRegexVariantToSuffix(variant), ".",
                      SendToDeviceToSuffix(sent_to_device)}),
        result);
  }
}

}  // namespace

ScopedUmaHistogramMicrosecondsTimer::ScopedUmaHistogramMicrosecondsTimer(
    PhoneNumberRegexVariant variant)
    : variant_(variant) {}

ScopedUmaHistogramMicrosecondsTimer::~ScopedUmaHistogramMicrosecondsTimer() {
  constexpr char kPrefix[] =
      "Sharing.ClickToCallContextMenuPhoneNumberParsingDelay";
  constexpr base::TimeDelta kMinTime = base::TimeDelta::FromMicroseconds(1);
  constexpr base::TimeDelta kMaxTime = base::TimeDelta::FromSeconds(1);
  constexpr int kBuckets = 50;

  base::TimeDelta elapsed = timer_.Elapsed();
  // Default bucket for all variants.
  base::UmaHistogramCustomMicrosecondsTimes(kPrefix, elapsed, kMinTime,
                                            kMaxTime, kBuckets);
  base::UmaHistogramCustomMicrosecondsTimes(
      base::StrCat({kPrefix, ".", PhoneNumberRegexVariantToSuffix(variant_)}),
      elapsed, kMinTime, kMaxTime, kBuckets);
}

void LogClickToCallHelpTextClicked(SharingDialogType type) {
  base::UmaHistogramEnumeration("Sharing.ClickToCallHelpTextClicked", type);
}

void LogClickToCallUKM(content::WebContents* web_contents,
                       SharingClickToCallEntryPoint entry_point,
                       bool has_devices,
                       bool has_apps,
                       SharingClickToCallSelection selection) {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return;

  ukm::SourceId source_id =
      ukm::GetSourceIdForWebContentsDocument(web_contents);
  if (source_id == ukm::kInvalidSourceId)
    return;

  ukm::builders::Sharing_ClickToCall(source_id)
      .SetEntryPoint(static_cast<int64_t>(entry_point))
      .SetHasDevices(has_devices)
      .SetHasApps(has_apps)
      .SetSelection(static_cast<int64_t>(selection))
      .Record(ukm_recorder);
}

void LogClickToCallPhoneNumberSize(const std::string& number,
                                   SharingClickToCallEntryPoint entry_point,
                                   bool send_to_device) {
  int length = number.size();
  int digits = std::count_if(number.begin(), number.end(),
                             [](char c) { return std::isdigit(c); });

  std::string suffix =
      base::StrCat({ClickToCallEntryPointToSuffix(entry_point), ".",
                    SendToDeviceToSuffix(send_to_device)});

  base::UmaHistogramCounts100("Sharing.ClickToCallPhoneNumberLength", length);
  base::UmaHistogramCounts100(
      base::StrCat({"Sharing.ClickToCallPhoneNumberLength.", suffix}), length);

  base::UmaHistogramCounts100("Sharing.ClickToCallPhoneNumberDigits", digits);
  base::UmaHistogramCounts100(
      base::StrCat({"Sharing.ClickToCallPhoneNumberDigits.", suffix}), digits);
}

void LogPhoneNumberDetectionMetrics(const std::string& selection_text,
                                    bool sent_to_device) {
  if (!base::FeatureList::IsEnabled(kClickToCallDetectionV2))
    return;

  base::PostTask(FROM_HERE,
                 {base::ThreadPool(), base::TaskPriority::BEST_EFFORT,
                  base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                 base::BindOnce(&LogPhoneNumberDetectionMetricsOnThreadPool,
                                selection_text, sent_to_device));
}
