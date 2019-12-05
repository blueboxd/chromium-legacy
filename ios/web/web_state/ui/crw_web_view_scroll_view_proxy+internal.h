// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WEB_VIEW_SCROLL_VIEW_PROXY_INTERNAL_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WEB_VIEW_SCROLL_VIEW_PROXY_INTERNAL_H_

#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

@class CRBProtocolObservers;

// Declares internal API for this class. This API should only be used in
// //ios/web.
@interface CRWWebViewScrollViewProxy (Internal)

// Observers of this proxy which subscribe to change notifications.
@property(nonatomic, readonly)
    CRBProtocolObservers<CRWWebViewScrollViewProxyObserver>* observers;

// The underlying UIScrollView. It can change or become nil.
@property(nonatomic, weak, readonly) UIScrollView* underlyingScrollView;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WEB_VIEW_SCROLL_VIEW_PROXY_INTERNAL_H_
