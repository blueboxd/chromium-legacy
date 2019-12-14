// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_TEST_FAKE_INFOBAR_UI_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_TEST_FAKE_INFOBAR_UI_DELEGATE_H_

#import "ios/chrome/browser/ui/infobars/infobar_ui_delegate.h"

// Fake implementation of InfobarUIDelegate to use in tests.
@interface FakeInfobarUIDelegate : NSObject <InfobarUIDelegate>

// Redefine InfobarUIDelegate property as readwrite.
@property(nonatomic, readwrite) BOOL hasBadge;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_TEST_FAKE_INFOBAR_UI_DELEGATE_H_
