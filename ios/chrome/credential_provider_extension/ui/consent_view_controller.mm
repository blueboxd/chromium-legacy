// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/consent_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ConsentViewController

#pragma mark - Public

- (void)loadView {
  self.image = [UIImage imageNamed:@"consent_illustration"];
  self.helpButtonAvailable = YES;
  self.primaryActionAvailable = YES;
  NSString* titleString =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_CONSENT_TITLE",
                        @"The title in the consent screen.");
  NSString* subtitleString =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_CONSENT_SUBTITLE",
                        @"The subtitle in the consent screen.");
  NSString* primaryActionString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_CONSENT_ENABLE_BUTTON_TITLE",
      @"The primary action title in the consent screen. Used to explicitly "
      @"enable the extension.");
  self.titleString = titleString;
  self.subtitleString = subtitleString;
  self.primaryActionString = primaryActionString;
#if defined(__IPHONE_13_4)
  if (@available(iOS 13.4, *)) {
    self.pointerInteractionEnabled = YES;
  }
#endif  // defined(__IPHONE_13_4)
  [super loadView];
}

@end
