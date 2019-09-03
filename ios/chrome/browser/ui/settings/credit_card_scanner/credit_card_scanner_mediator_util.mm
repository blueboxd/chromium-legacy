// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_mediator_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {

NSDateComponents* ExtractExpirationDateFromText(NSString* text) {
  NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
  [formatter setDateFormat:@"MM/yy"];
  NSDate* date = [formatter dateFromString:text];

  if (date) {
    NSCalendar* gregorian = [[NSCalendar alloc]
        initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
    return [gregorian components:NSCalendarUnitMonth | NSCalendarUnitYear
                        fromDate:date];
  }

  return nil;
}

}  // namespace ios
