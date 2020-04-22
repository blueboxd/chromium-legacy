// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/cookies_mediator.h"

#import "ios/chrome/browser/ui/settings/privacy/cookies_consumer.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PrivacyCookiesMediator

- (void)setConsumer:(id<PrivacyCookiesConsumer>)consumer {
  if (_consumer == consumer)
    return;

  _consumer = consumer;
  [self updateConsumer];
}

#pragma mark - Private

- (void)updateConsumer {
  // TODO(crbug.com/1064961): Implement this.
  [self.consumer cookiesSettingsOptionSelected:SettingTypeAllowCookies];
  [self.consumer insertSiteExceptionsItems:[self populateSiteExceptions]];
}

- (NSArray*)populateSiteExceptions {
  NSMutableArray* items = [[NSMutableArray alloc] init];
  // TODO(crbug.com/1064961): Implement this.

  return items;
}

@end
