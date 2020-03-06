// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_mediator.h"

#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/reading_list/offline_page_tab_helper.h"
#import "ios/chrome/browser/ui/page_info/page_info_consumer.h"
#import "ios/chrome/browser/ui/page_info/page_info_description.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PageInfoMediator ()
@property(nonatomic, assign) web::WebState* webState;
@property(nonatomic, assign) web::NavigationItem* navigationItem;
@end

@implementation PageInfoMediator

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    _webState = webState;
    _navigationItem = _webState->GetNavigationManager()->GetVisibleItem();
  }
  return self;
}

- (void)setConsumer:(id<PageInfoConsumer>)consumer {
  if (_consumer == consumer)
    return;

  _consumer = consumer;
  [self updateConsumer];
}

#pragma mark - Private

- (void)updateConsumer {
  const BOOL presentingOfflinePage =
      OfflinePageTabHelper::FromWebState(_webState)->presenting_offline_page();

  PageInfoDescription* description =
      [[PageInfoDescription alloc] initWithURL:self.navigationItem->GetURL()
                                     SSLStatus:self.navigationItem->GetSSL()
                                 isPageOffline:presentingOfflinePage];
  [self.consumer pageInfoChanged:description];
}

@end
