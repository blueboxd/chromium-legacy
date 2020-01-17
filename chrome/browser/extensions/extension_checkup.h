// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_CHECKUP_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_CHECKUP_H_

namespace content {
class BrowserContext;
}

namespace extensions {
// The type of message the extensions checkup banner will convey. These values
// are persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class CheckupMessage {
  // A performance focused message.
  PERFORMANCE = 0,
  // A privacy focused message.
  PRIVACY = 1,
  // A neutral message.
  NEUTRAL = 2,
  kMaxValue = NEUTRAL
};

// Returns true if the user should be shown the extensions page and checkup
// banner upon startup.
bool ShouldShowExtensionsCheckupOnStartup(content::BrowserContext* context);

// Returns true if the user should be shown the extensions checkup promo in
// the NTP.
bool ShouldShowExtensionsCheckupPromo(content::BrowserContext* context);

// Returns an enum representing the type of message the checkup banner will
// convey.
CheckupMessage GetCheckupMessageFocus();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_CHECKUP_H_
