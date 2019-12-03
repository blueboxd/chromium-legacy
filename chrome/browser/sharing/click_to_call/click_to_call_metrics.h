// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_METRICS_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_METRICS_H_

#include <string>

#include "base/timer/elapsed_timer.h"
#include "chrome/browser/sharing/click_to_call/phone_number_regex.h"
#include "chrome/browser/sharing/sharing_metrics.h"

namespace content {
class WebContents;
}  // namespace content

// Entry point of a Click to Call journey.
// These values are logged to UKM. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SharingClickToCallEntryPoint" in src/tools/metrics/histograms/enums.xml.
enum class SharingClickToCallEntryPoint {
  kLeftClickLink = 0,
  kRightClickLink = 1,
  kRightClickSelection = 2,
  kMaxValue = kRightClickSelection,
};

// Selection at the end of a Click to Call journey.
// These values are logged to UKM. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SharingClickToCallSelection" in src/tools/metrics/histograms/enums.xml.
enum class SharingClickToCallSelection {
  kNone = 0,
  kDevice = 1,
  kApp = 2,
  kMaxValue = kApp,
};

// Result of comparing the simple regex with one of the available variants.
// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "PhoneNumberRegexVariantResult" in src/tools/metrics/histograms/enums.xml.
enum class PhoneNumberRegexVariantResult {
  kNoneMatch = 0,
  kOnlySimpleMatches = 1,
  kOnlyVariantMatches = 2,
  kBothMatch = 3,
  kMaxValue = kBothMatch,
};

// TODO(himanshujaju): Make it generic and move to base/metrics/histogram_base.h
// Used to Log delay in parsing phone number in highlighted text to UMA.
class ScopedUmaHistogramMicrosecondsTimer {
 public:
  explicit ScopedUmaHistogramMicrosecondsTimer(PhoneNumberRegexVariant variant);
  ScopedUmaHistogramMicrosecondsTimer(
      const ScopedUmaHistogramMicrosecondsTimer&) = delete;
  ScopedUmaHistogramMicrosecondsTimer& operator=(
      const ScopedUmaHistogramMicrosecondsTimer&) = delete;
  ~ScopedUmaHistogramMicrosecondsTimer();

 private:
  const PhoneNumberRegexVariant variant_;
  const base::ElapsedTimer timer_;
};

// Logs the dialog type when a user clicks on the help text in the Click to Call
// dialog.
void LogClickToCallHelpTextClicked(SharingDialogType type);

// Records a Click to Call selection to UKM. This is logged after a completed
// action like selecting an app or a device to send the phone number to.
void LogClickToCallUKM(content::WebContents* web_contents,
                       SharingClickToCallEntryPoint entry_point,
                       bool has_devices,
                       bool has_apps,
                       SharingClickToCallSelection selection);

// Logs the raw phone number length and the number of digits in it.
void LogClickToCallPhoneNumberSize(const std::string& number,
                                   SharingClickToCallEntryPoint entry_point,
                                   bool send_to_device);

// Compares alternative regexes to the default one on a low priority task runner
// and records the results to UMA.
void LogPhoneNumberDetectionMetrics(const std::string& selection_text,
                                    bool sent_to_device);

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_METRICS_H_
