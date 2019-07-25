// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_button_action_handler.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/badges/badge_button.h"
#import "ios/chrome/browser/ui/commands/infobar_commands.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BadgeButtonActionHandler

- (void)passwordsBadgeButtonTapped:(id)sender {
  BadgeButton* badgeButton = base::mac::ObjCCastStrict<BadgeButton>(sender);
  if (badgeButton.badgeType == BadgeType::kBadgeTypePasswordSave) {
    [self.dispatcher displayModalInfobar];
  } else if (badgeButton.badgeType == BadgeType::kBadgeTypePasswordUpdate) {
    [self.dispatcher displayModalInfobar];
  }
}

@end
