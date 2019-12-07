// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_view_scroll_view_proxy+internal.h"

#import <objc/runtime.h>

#include <memory>

#include "base/auto_reset.h"
#import "base/ios/crb_protocol_observers.h"
#include "base/mac/foundation_util.h"
#import "ios/web/web_state/ui/crw_web_view_scroll_view_delegate_proxy.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWWebViewScrollViewProxy () {
  std::unique_ptr<UIScrollViewContentInsetAdjustmentBehavior>
      _storedContentInsetAdjustmentBehavior API_AVAILABLE(ios(11.0));
  std::unique_ptr<BOOL> _storedClipsToBounds;
}

// A delegate object of the UIScrollView managed by this class.
@property(nonatomic, strong, readonly)
    CRWWebViewScrollViewDelegateProxy* delegateProxy;

@property(nonatomic, strong)
    CRBProtocolObservers<CRWWebViewScrollViewProxyObserver>* observers;
@property(nonatomic, weak) UIScrollView* underlyingScrollView;

// This exists for compatibility with UIScrollView (see -asUIScrollView).
@property(nonatomic, weak) id<UIScrollViewDelegate> delegate;

// Returns the key paths that need to be observed for UIScrollView.
+ (NSArray*)scrollViewObserverKeyPaths;

// Adds and removes |self| as an observer for |scrollView| with key paths
// returned by |+scrollViewObserverKeyPaths|.
- (void)startObservingScrollView:(UIScrollView*)scrollView;
- (void)stopObservingScrollView:(UIScrollView*)scrollView;

@end

// Note: An instance of this class must be safely casted to UIScrollView. See
// -asUIScrollView. To make it happen:
//   - When this class defines a method with the same selector as in a method of
//     UIScrollView (or its ancestor classes), its API and the behavior should
//     be consistent with the UIScrollView one's.
//   - Calls to UIScrollView methods not implemented in this class are forwarded
//     to the underlying UIScrollView by -methodSignatureForSelector: and
//     -forwardInvocation:.
@implementation CRWWebViewScrollViewProxy

- (instancetype)init {
  self = [super init];
  if (self) {
    Protocol* protocol = @protocol(CRWWebViewScrollViewProxyObserver);
    _observers =
        static_cast<CRBProtocolObservers<CRWWebViewScrollViewProxyObserver>*>(
            [CRBProtocolObservers observersWithProtocol:protocol]);
    _delegateProxy = [[CRWWebViewScrollViewDelegateProxy alloc]
        initWithScrollViewProxy:self];
  }
  return self;
}

- (void)dealloc {
  [self stopObservingScrollView:self.underlyingScrollView];
}

- (void)addGestureRecognizer:(UIGestureRecognizer*)gestureRecognizer {
  [self.underlyingScrollView addGestureRecognizer:gestureRecognizer];
}

- (void)removeGestureRecognizer:(UIGestureRecognizer*)gestureRecognizer {
  [self.underlyingScrollView removeGestureRecognizer:gestureRecognizer];
}

- (void)addObserver:(id<CRWWebViewScrollViewProxyObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<CRWWebViewScrollViewProxyObserver>)observer {
  [_observers removeObserver:observer];
}

- (void)setScrollView:(UIScrollView*)scrollView {
  if (self.underlyingScrollView == scrollView)
    return;
  [self.underlyingScrollView setDelegate:nil];
  [self stopObservingScrollView:self.underlyingScrollView];
  DCHECK(!scrollView.delegate);
  scrollView.delegate = self.delegateProxy;
  [self startObservingScrollView:scrollView];
  self.underlyingScrollView = scrollView;
  if (_storedClipsToBounds) {
    scrollView.clipsToBounds = *_storedClipsToBounds;
  }

  // Assigns |contentInsetAdjustmentBehavior| which was set before setting the
  // scroll view.
  if (_storedContentInsetAdjustmentBehavior) {
    self.underlyingScrollView.contentInsetAdjustmentBehavior =
        *_storedContentInsetAdjustmentBehavior;
  }

  [_observers webViewScrollViewProxyDidSetScrollView:self];
}

- (CGRect)frame {
  return self.underlyingScrollView ? [self.underlyingScrollView frame]
                                   : CGRectZero;
}

- (BOOL)isScrollEnabled {
  return [self.underlyingScrollView isScrollEnabled];
}

- (void)setScrollEnabled:(BOOL)scrollEnabled {
  [self.underlyingScrollView setScrollEnabled:scrollEnabled];
}

- (BOOL)bounces {
  return [self.underlyingScrollView bounces];
}

- (void)setBounces:(BOOL)bounces {
  [self.underlyingScrollView setBounces:bounces];
}

- (BOOL)clipsToBounds {
  if (!self.underlyingScrollView && _storedClipsToBounds) {
    return *_storedClipsToBounds;
  }
  return self.underlyingScrollView.clipsToBounds;
}

- (void)setClipsToBounds:(BOOL)clipsToBounds {
  _storedClipsToBounds = std::make_unique<BOOL>(clipsToBounds);
  self.underlyingScrollView.clipsToBounds = clipsToBounds;
}

- (BOOL)isDecelerating {
  return [self.underlyingScrollView isDecelerating];
}

- (BOOL)isDragging {
  return [self.underlyingScrollView isDragging];
}

- (BOOL)isTracking {
  return [self.underlyingScrollView isTracking];
}

- (BOOL)isZooming {
  return [self.underlyingScrollView isZooming];
}

- (CGFloat)zoomScale {
  return [self.underlyingScrollView zoomScale];
}

- (void)setContentOffset:(CGPoint)contentOffset {
  [self.underlyingScrollView setContentOffset:contentOffset];
}

- (CGPoint)contentOffset {
  return self.underlyingScrollView ? [self.underlyingScrollView contentOffset]
                                   : CGPointZero;
}

- (void)setContentInset:(UIEdgeInsets)contentInset {
  [self.underlyingScrollView setContentInset:contentInset];
}

- (UIEdgeInsets)contentInset {
  return self.underlyingScrollView ? [self.underlyingScrollView contentInset]
                                   : UIEdgeInsetsZero;
}

- (void)setScrollIndicatorInsets:(UIEdgeInsets)scrollIndicatorInsets {
  [self.underlyingScrollView setScrollIndicatorInsets:scrollIndicatorInsets];
}

- (UIEdgeInsets)scrollIndicatorInsets {
  return self.underlyingScrollView
             ? [self.underlyingScrollView scrollIndicatorInsets]
             : UIEdgeInsetsZero;
}

- (void)setContentSize:(CGSize)contentSize {
  [self.underlyingScrollView setContentSize:contentSize];
}

- (CGSize)contentSize {
  return self.underlyingScrollView ? [self.underlyingScrollView contentSize]
                                   : CGSizeZero;
}

- (void)setContentOffset:(CGPoint)contentOffset animated:(BOOL)animated {
  [self.underlyingScrollView setContentOffset:contentOffset animated:animated];
}

- (BOOL)scrollsToTop {
  return [self.underlyingScrollView scrollsToTop];
}

- (void)setScrollsToTop:(BOOL)scrollsToTop {
  [self.underlyingScrollView setScrollsToTop:scrollsToTop];
}

- (UIScrollViewContentInsetAdjustmentBehavior)contentInsetAdjustmentBehavior
    API_AVAILABLE(ios(11.0)) {
  if (self.underlyingScrollView) {
    return [self.underlyingScrollView contentInsetAdjustmentBehavior];
  } else if (_storedContentInsetAdjustmentBehavior) {
    return *_storedContentInsetAdjustmentBehavior;
  } else {
    return UIScrollViewContentInsetAdjustmentAutomatic;
  }
}

- (UIEdgeInsets)adjustedContentInset API_AVAILABLE(ios(11.0)) {
  return [self.underlyingScrollView adjustedContentInset];
}

- (void)setContentInsetAdjustmentBehavior:
    (UIScrollViewContentInsetAdjustmentBehavior)contentInsetAdjustmentBehavior
    API_AVAILABLE(ios(11.0)) {
  [self.underlyingScrollView
      setContentInsetAdjustmentBehavior:contentInsetAdjustmentBehavior];
  _storedContentInsetAdjustmentBehavior =
      std::make_unique<UIScrollViewContentInsetAdjustmentBehavior>(
          contentInsetAdjustmentBehavior);
}

- (UIPanGestureRecognizer*)panGestureRecognizer {
  return [self.underlyingScrollView panGestureRecognizer];
}

- (NSArray*)gestureRecognizers {
  return [self.underlyingScrollView gestureRecognizers];
}

- (NSArray<__kindof UIView*>*)subviews {
  return self.underlyingScrollView ? [self.underlyingScrollView subviews] : @[];
}

#pragma mark -

+ (NSArray*)scrollViewObserverKeyPaths {
  return @[ @"frame", @"contentSize", @"contentInset" ];
}

- (void)startObservingScrollView:(UIScrollView*)scrollView {
  for (NSString* keyPath in [[self class] scrollViewObserverKeyPaths])
    [scrollView addObserver:self forKeyPath:keyPath options:0 context:nil];
}

- (void)stopObservingScrollView:(UIScrollView*)scrollView {
  for (NSString* keyPath in [[self class] scrollViewObserverKeyPaths])
    [scrollView removeObserver:self forKeyPath:keyPath];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  DCHECK_EQ(object, self.underlyingScrollView);
  if ([keyPath isEqualToString:@"frame"])
    [_observers webViewScrollViewFrameDidChange:self];
  if ([keyPath isEqualToString:@"contentSize"])
    [_observers webViewScrollViewDidResetContentSize:self];
  if ([keyPath isEqualToString:@"contentInset"])
    [_observers webViewScrollViewDidResetContentInset:self];
}

- (UIScrollView*)asUIScrollView {
  // See the comment of @implementation of this class for why this should be
  // safe.
  return (UIScrollView*)self;
}

#pragma mark - Forwards unimplemented UIScrollView methods

- (NSMethodSignature*)methodSignatureForSelector:(SEL)sel {
  // Called when this proxy is accessed through -asUIScrollView and the method
  // is not implemented in this class. Do not call [self.underlyingScrollView
  // methodSignatureForSelector:] here instead because self.underlyingScrollView
  // may be nil.
  return [UIScrollView instanceMethodSignatureForSelector:sel];
}

- (void)forwardInvocation:(NSInvocation*)invocation {
  // Called when this proxy is accessed through -asUIScrollView and the method
  // is not implemented in this class. self.underlyingScrollView may be nil, but
  // it is safe. nil can respond to any invocation (and does nothing).
  [invocation invokeWithTarget:self.underlyingScrollView];
}

#pragma mark - NSObject

- (BOOL)isKindOfClass:(Class)aClass {
  // Pretend self to be a kind of UIScrollView.
  return
      [UIScrollView isSubclassOfClass:aClass] || [super isKindOfClass:aClass];
}

- (BOOL)respondsToSelector:(SEL)aSelector {
  // Respond to both of UIScrollView methods and its own methods.
  return [UIScrollView instancesRespondToSelector:aSelector] ||
         [super respondsToSelector:aSelector];
}

@end
