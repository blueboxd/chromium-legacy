// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UTILS_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UTILS_H_

#include <string>

#include "base/optional.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

// Returns true if click to call feature should be shown for |url|.
bool ShouldOfferClickToCallForURL(content::BrowserContext* browser_context,
                                  const GURL& url);

// Returns the first possible phone number in |selection_text| if click to call
// should be offered. Otherwise returns base::nullopt.
base::Optional<std::string> ExtractPhoneNumberForClickToCall(
    content::BrowserContext* browser_context,
    const std::string& selection_text);

// Returns the first possible phone number in |selection_text| given the
// |regex_variant| to be used or base::nullopt if the regex did not match.
base::Optional<std::string> ExtractPhoneNumber(
    const std::string& selection_text);

// Unescapes and returns the URL contents.
std::string GetUnescapedURLContent(const GURL& url);

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UTILS_H_
