// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace {

// Upper bound on length of selection text to avoid performance issues. Take
// into account numbers with country code and spaces '+99 0 999 999 9999' when
// reducing the max length.
constexpr int kSelectionTextMaxLength = 30;

// Heuristical regex to search for phone number.
// (^|\p{Z}) makes sure the pattern begins with a new word.
// (\(?\+[0-9]+\)?) checks for optional international code in number.
// ([.\p{Z}\-\(]?[0-9][\p{Z}\-\)]?){8,} checks for at least eight occurrences of
// digits with optional separators to reduce false positives.
const char kPhoneNumberRegexPattern[] =
    R"((?:^|\p{Z})((?:\(?\+[0-9]+\)?)?(?:[.\p{Z}\-(]?[0-9][\p{Z}\-)]?){8,}))";

bool IsClickToCallEnabled(content::BrowserContext* browser_context) {
  SharingService* sharing_service =
      SharingServiceFactory::GetForBrowserContext(browser_context);
  return sharing_service && base::FeatureList::IsEnabled(kClickToCallUI);
}

// Todo(himanshujaju): Make it generic and move to base/metrics/histogram_base.h
// Used to Log delay in parsing phone number in highlighted text to UMA.
struct ScopedUmaHistogramMicrosecondsTimer {
  ScopedUmaHistogramMicrosecondsTimer() : timer() {}

  ~ScopedUmaHistogramMicrosecondsTimer() {
    base::UmaHistogramCustomMicrosecondsTimes(
        "Sharing.ClickToCallContextMenuPhoneNumberParsingDelay",
        timer.Elapsed(), base::TimeDelta::FromMicroseconds(1),
        base::TimeDelta::FromSeconds(1), 50);
  }

  const base::ElapsedTimer timer;
};

}  // namespace

bool ShouldOfferClickToCallForURL(content::BrowserContext* browser_context,
                                  const GURL& url) {
  return !url.is_empty() && url.SchemeIs(url::kTelScheme) &&
         !url.GetContent().empty() && IsClickToCallEnabled(browser_context);
}

base::Optional<std::string> ExtractPhoneNumberForClickToCall(
    content::BrowserContext* browser_context,
    const std::string& selection_text) {
  DCHECK(!selection_text.empty());

  if (selection_text.size() > kSelectionTextMaxLength)
    return base::nullopt;

  if (!base::FeatureList::IsEnabled(kClickToCallContextMenuForSelectedText) ||
      !IsClickToCallEnabled(browser_context)) {
    return base::nullopt;
  }

  ScopedUmaHistogramMicrosecondsTimer scoped_uma_timer;

  // TODO(crbug.com/992906): Find a better way to parse phone numbers.
  static const re2::LazyRE2 kPhoneNumberRegex = {kPhoneNumberRegexPattern};
  std::string parsed_phone_number;
  if (!re2::RE2::PartialMatch(selection_text, *kPhoneNumberRegex,
                              &parsed_phone_number)) {
    return base::nullopt;
  }

  return base::UTF16ToUTF8(base::TrimWhitespace(
      base::UTF8ToUTF16(parsed_phone_number), base::TRIM_ALL));
}

std::string GetUnescapedURLContent(const GURL& url) {
  std::string content_string(url.GetContent());
  url::RawCanonOutputT<base::char16> unescaped_content;
  url::DecodeURLEscapeSequences(content_string.data(), content_string.size(),
                                url::DecodeURLMode::kUTF8OrIsomorphic,
                                &unescaped_content);
  return base::UTF16ToUTF8(
      base::string16(unescaped_content.data(), unescaped_content.length()));
}
