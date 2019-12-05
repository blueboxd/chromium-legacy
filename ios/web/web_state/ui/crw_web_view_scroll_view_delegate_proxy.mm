// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_view_scroll_view_delegate_proxy.h"

#import "base/ios/crb_protocol_observers.h"
#include "base/logging.h"
#import "ios/web/web_state/ui/crw_web_view_scroll_view_proxy+internal.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWWebViewScrollViewDelegateProxy ()

@property(nonatomic) CRWWebViewScrollViewProxy* scrollViewProxy;

@end

// TODO(crbug.com/1023250): Delegate to _scrollViewProxy.delegate as well.
@implementation CRWWebViewScrollViewDelegateProxy

- (instancetype)initWithScrollViewProxy:
    (CRWWebViewScrollViewProxy*)scrollViewProxy {
  self = [super init];
  if (self) {
    _scrollViewProxy = scrollViewProxy;
  }
  return self;
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  [self.scrollViewProxy.observers
      webViewScrollViewDidScroll:self.scrollViewProxy];
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  [self.scrollViewProxy.observers
      webViewScrollViewWillBeginDragging:self.scrollViewProxy];
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  [self.scrollViewProxy.observers
      webViewScrollViewWillEndDragging:self.scrollViewProxy
                          withVelocity:velocity
                   targetContentOffset:targetContentOffset];
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  [self.scrollViewProxy.observers
      webViewScrollViewDidEndDragging:self.scrollViewProxy
                       willDecelerate:decelerate];
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  [self.scrollViewProxy.observers
      webViewScrollViewDidEndDecelerating:self.scrollViewProxy];
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView*)scrollView {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  [self.scrollViewProxy.observers
      webViewScrollViewDidEndScrollingAnimation:self.scrollViewProxy];
}

- (BOOL)scrollViewShouldScrollToTop:(UIScrollView*)scrollView {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  __block BOOL shouldScrollToTop = YES;
  [self.scrollViewProxy.observers executeOnObservers:^(id observer) {
    if ([observer respondsToSelector:@selector
                  (webViewScrollViewShouldScrollToTop:)]) {
      shouldScrollToTop =
          shouldScrollToTop &&
          [observer webViewScrollViewShouldScrollToTop:self.scrollViewProxy];
    }
  }];
  return shouldScrollToTop;
}

- (void)scrollViewDidZoom:(UIScrollView*)scrollView {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  [self.scrollViewProxy.observers
      webViewScrollViewDidZoom:self.scrollViewProxy];
}

- (void)scrollViewWillBeginZooming:(UIScrollView*)scrollView
                          withView:(UIView*)view {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  [self.scrollViewProxy.observers
      webViewScrollViewWillBeginZooming:self.scrollViewProxy];
}

- (void)scrollViewDidEndZooming:(UIScrollView*)scrollView
                       withView:(UIView*)view
                        atScale:(CGFloat)scale {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  [self.scrollViewProxy.observers
      webViewScrollViewDidEndZooming:self.scrollViewProxy
                             atScale:scale];
}

@end
