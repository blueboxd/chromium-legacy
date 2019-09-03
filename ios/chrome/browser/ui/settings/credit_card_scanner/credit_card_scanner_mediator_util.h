// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_MEDIATOR_UTIL_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_MEDIATOR_UTIL_H_

#import <Foundation/Foundation.h>

namespace ios {

// Extracts credit card expiration month and year from |text|.
NSDateComponents* ExtractExpirationDateFromText(NSString* text);

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_MEDIATOR_UTIL_H_
