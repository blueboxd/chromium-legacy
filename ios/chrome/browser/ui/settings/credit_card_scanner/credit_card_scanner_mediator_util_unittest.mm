// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_mediator_util.h"

#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using CreditCardScannerMediatorUtilTest = PlatformTest;

// Tests extracting month and year from valid date text.
TEST_F(CreditCardScannerMediatorUtilTest,
       TestExtractExpirationDateFromValidDateText) {
  NSDateComponents* components = ios::ExtractExpirationDateFromText(@"10/25");

  EXPECT_EQ(components.month, 10);
  EXPECT_EQ(components.year, 2025);
}

// Tests extracting month and year from invalid date text.
TEST_F(CreditCardScannerMediatorUtilTest,
       TestExtractExpirationDateFromInvalidDateText) {
  NSDateComponents* components = ios::ExtractExpirationDateFromText(@"13/888");

  EXPECT_FALSE(components);
}

// Tests extracting month and year from invalid text.
TEST_F(CreditCardScannerMediatorUtilTest,
       TestExtractExpirationDateFromInvalidText) {
  NSDateComponents* components = ios::ExtractExpirationDateFromText(@"aaaaa");

  EXPECT_FALSE(components);
}

// Tests extracting month and year from invalid text with correct format.
TEST_F(CreditCardScannerMediatorUtilTest,
       TestExtractExpirationDateFromInvalidFormattedText) {
  NSDateComponents* components = ios::ExtractExpirationDateFromText(@"aa/aa");

  EXPECT_FALSE(components);
}
