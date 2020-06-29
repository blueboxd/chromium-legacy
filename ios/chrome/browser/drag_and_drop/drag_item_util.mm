// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drag_and_drop/drag_item_util.h"

#include "base/check_op.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/browser/window_activities/window_activity_helpers.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"
#include "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabInfo
- (instancetype)initWithTabID:(NSString*)tabID incognito:(BOOL)incognito {
  self = [super init];
  if (self) {
    _tabID = tabID;
    _incognito = incognito;
  }
  return self;
}
@end

UIDragItem* CreateTabDragItem(web::WebState* web_state) {
  DCHECK(web_state);
  NSURL* url = net::NSURLWithGURL(web_state->GetVisibleURL());
  NSItemProvider* item_provider = [[NSItemProvider alloc] initWithObject:url];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  NSString* tab_id = TabIdTabHelper::FromWebState(web_state)->tab_id();
  // Visibility "all" is required to allow the OS to recognize this activity for
  // creating a new window.
  [item_provider registerObject:ActivityToMoveTab(tab_id)
                     visibility:NSItemProviderRepresentationVisibilityAll];
  TabInfo* tab_info = [[TabInfo alloc]
      initWithTabID:tab_id
          incognito:web_state->GetBrowserState()->IsOffTheRecord()];
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  drag_item.localObject = tab_info;
  return drag_item;
}

UIDragItem* CreateURLDragItem(const GURL& url, WindowActivityOrigin origin) {
  DCHECK(url.is_valid());
  NSURL* url_object = net::NSURLWithGURL(url);
  NSItemProvider* item_provider =
      [[NSItemProvider alloc] initWithObject:url_object];
  // Visibility "all" is required to allow the OS to recognize this activity for
  // creating a new window.
  [item_provider registerObject:ActivityToLoadURL(origin, url)
                     visibility:NSItemProviderRepresentationVisibilityAll];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  drag_item.localObject = url_object;
  return drag_item;
}
