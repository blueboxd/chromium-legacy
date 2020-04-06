// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_view_scroll_view_proxy+internal.h"

#import <objc/runtime.h>
#include <memory>

#include "base/auto_reset.h"
#import "base/ios/crb_protocol_observers.h"
#include "base/mac/foundation_util.h"
#include "ios/web/common/features.h"
#import "ios/web/web_state/ui/crw_web_view_scroll_view_delegate_proxy.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A wrapper of a key-value observer. When an instance of
// CRWKeyValueObserverForwarder receives a KVO callback, it forwards the
// callback to |wrappedObserver|, but replacing the object parameter with the
// |object| given in its initializer.
//
// This is useful when creating a proxy class of an object and forwarding KVO
// against the proxy object to the underlying object, but making the KVO
// callback still look like a callback from the proxy object.
@interface CRWKeyValueObserverForwarder : NSObject

// The number of times when -addObserver:forKeyPath:options:context: has been
// called with this wrapper. The caller of this class is responsible to update
// this.
@property(nonatomic) int observationCount;

@property(nonatomic, weak) id wrappedObserver;
@property(nonatomic, weak) id object;

- (instancetype)initWithWrappedObserver:(id)wrappedObserver
                                 object:(id)object NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

@implementation CRWKeyValueObserverForwarder

- (instancetype)initWithWrappedObserver:(id)wrappedObserver object:(id)object {
  self = [super init];
  if (self) {
    _wrappedObserver = wrappedObserver;
    _object = object;
  }
  return self;
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  [self.wrappedObserver observeValueForKeyPath:keyPath
                                      ofObject:self.object
                                        change:change
                                       context:context];
}

@end

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

@property(nonatomic, strong) UIScrollView* underlyingScrollView;

// This exists for compatibility with UIScrollView (see -asUIScrollView).
@property(nonatomic, weak) id<UIScrollViewDelegate> delegate;

// YES while key-value observers are registered on the underlying UIScrollView.
@property(nonatomic) BOOL observingScrollView;

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
@implementation CRWWebViewScrollViewProxy {
  // Wrappers of key-value observers against this instance, keyed by the key
  // path (the outer dictionary) and the observer (the inner map table).
  NSMutableDictionary<NSString*,
                      NSMapTable<id, CRWKeyValueObserverForwarder*>*>*
      _keyValueObserverForwarders;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    Protocol* protocol = @protocol(CRWWebViewScrollViewProxyObserver);
    _observers =
        static_cast<CRBProtocolObservers<CRWWebViewScrollViewProxyObserver>*>(
            [CRBProtocolObservers observersWithProtocol:protocol]);
    _delegateProxy = [[CRWWebViewScrollViewDelegateProxy alloc]
        initWithScrollViewProxy:self];
    _keyValueObserverForwarders = [[NSMutableDictionary alloc] init];

    // Assign a placeholder UIScrollView until the actual underlying scroll view
    // is set. This must be a real UIScrollView, not nil, so that:
    //   - The proxy preserves the values of properties assigned before the
    //     actual scroll view is set. These properties will then be inherited to
    //     the actual scroll view in -setScrollView:.
    //   - The proxy returns the actual default value of the property before the
    //     actual scroll view is set, even when the default value is non-zero
    //     e.g., scrollsToTop.
    //   - The proxy uses the actual implementation of methods defined in
    //     third-party categories of UIScrollView.
    //
    // Note that this proxy must support all methods/properties of UIScrollView,
    // including those defined in third-party categories, because it provides
    // -asUIScrollView method.
    _underlyingScrollView = [[UIScrollView alloc] init];
  }
  return self;
}

- (void)dealloc {
  [self stopObservingScrollView:self.underlyingScrollView];
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

  // Use a placeholder UIScrollView instead when nil is given. See the comment
  // in -init why this is necessary.
  if (!scrollView) {
    scrollView = [[UIScrollView alloc] init];
  }

  // Clean up the delegate/observers of the old scroll view.
  [self.underlyingScrollView setDelegate:nil];
  [self stopObservingScrollView:self.underlyingScrollView];

  // Set up the delegate/observers of the new scroll view.
  DCHECK(!scrollView.delegate);
  scrollView.delegate = self.delegateProxy;
  [self startObservingScrollView:scrollView];

  if (base::FeatureList::IsEnabled(
          web::features::kPreserveScrollViewProperties)) {
    [self preservePropertiesFromOldScrollView:self.underlyingScrollView
                              toNewScrollView:scrollView];
  }

  self.underlyingScrollView = scrollView;

  // TODO(crbug.com/1023250): Restore these in
  // -preservePropertiesFromOldScrollView:toNewScrollView: once the feature flag
  // kPreserveScrollViewProperties is removed.
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

// Preserves properties of the underlying scroll view when it changes from
// |oldScrollView| to |newScrollView|.
//
// This is necessary to avoid losing properties set against the proxy when the
// underlying scroll view is reset.
- (void)preservePropertiesFromOldScrollView:(UIScrollView*)oldScrollView
                            toNewScrollView:(UIScrollView*)newScrollView {
  // This method should preserve all properties of UIScrollView and its
  // ancestor classes (not limited to properties explicitly declared in
  // CRWWebViewScrollViewProxy) which:
  //   - is a readwrite property
  //   - AND is supposed to be modified directly, considering it's a scroll
  //     view of a web view. e.g., |frame| and |subviews| do not meet this
  //     condition because they are managed by the web view.
  //
  // Properties not explicitly declared in CRWWebViewScrollViewProxy can still
  // be accessed via -asUIScrollView, so they should be preserved as well.

  // UIScrollView properties.
  newScrollView.scrollEnabled = oldScrollView.scrollEnabled;
  newScrollView.directionalLockEnabled = oldScrollView.directionalLockEnabled;
  newScrollView.pagingEnabled = oldScrollView.pagingEnabled;
  newScrollView.scrollsToTop = oldScrollView.scrollsToTop;
  newScrollView.bounces = oldScrollView.bounces;
  newScrollView.alwaysBounceVertical = oldScrollView.alwaysBounceVertical;
  newScrollView.alwaysBounceHorizontal = oldScrollView.alwaysBounceHorizontal;
  newScrollView.showsHorizontalScrollIndicator =
      oldScrollView.showsHorizontalScrollIndicator;
  newScrollView.showsVerticalScrollIndicator =
      oldScrollView.showsVerticalScrollIndicator;
  newScrollView.canCancelContentTouches = oldScrollView.canCancelContentTouches;
  newScrollView.delaysContentTouches = oldScrollView.delaysContentTouches;
  newScrollView.keyboardDismissMode = oldScrollView.keyboardDismissMode;
  newScrollView.indexDisplayMode = oldScrollView.indexDisplayMode;
  newScrollView.indicatorStyle = oldScrollView.indicatorStyle;

  // UIView properties.
  newScrollView.backgroundColor = oldScrollView.backgroundColor;
  newScrollView.hidden = oldScrollView.hidden;
  newScrollView.alpha = oldScrollView.alpha;
  newScrollView.opaque = oldScrollView.opaque;
  newScrollView.tintColor = oldScrollView.tintColor;
  newScrollView.tintAdjustmentMode = oldScrollView.tintAdjustmentMode;
  newScrollView.clearsContextBeforeDrawing =
      oldScrollView.clearsContextBeforeDrawing;
  newScrollView.maskView = oldScrollView.maskView;
  newScrollView.userInteractionEnabled = oldScrollView.userInteractionEnabled;
  newScrollView.multipleTouchEnabled = oldScrollView.multipleTouchEnabled;
  newScrollView.exclusiveTouch = oldScrollView.exclusiveTouch;
}

- (BOOL)clipsToBounds {
  return self.underlyingScrollView.clipsToBounds;
}

- (void)setClipsToBounds:(BOOL)clipsToBounds {
  _storedClipsToBounds = std::make_unique<BOOL>(clipsToBounds);
  self.underlyingScrollView.clipsToBounds = clipsToBounds;
}

- (UIScrollViewContentInsetAdjustmentBehavior)contentInsetAdjustmentBehavior
    API_AVAILABLE(ios(11.0)) {
  return [self.underlyingScrollView contentInsetAdjustmentBehavior];
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

- (NSArray<__kindof UIView*>*)subviews {
  return [self.underlyingScrollView subviews];
}

#pragma mark -

+ (NSArray*)scrollViewObserverKeyPaths {
  return @[ @"frame", @"contentSize", @"contentInset" ];
}

- (void)startObservingScrollView:(UIScrollView*)scrollView {
  for (NSString* keyPath in [[self class] scrollViewObserverKeyPaths])
    [scrollView addObserver:self forKeyPath:keyPath options:0 context:nil];
  self.observingScrollView = YES;
}

- (void)stopObservingScrollView:(UIScrollView*)scrollView {
  if (!self.observingScrollView) {
    return;
  }
  for (NSString* keyPath in [[self class] scrollViewObserverKeyPaths])
    [scrollView removeObserver:self forKeyPath:keyPath];
  self.observingScrollView = NO;
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
  // is not implemented in this class.
  return [self.underlyingScrollView methodSignatureForSelector:sel];
}

- (void)forwardInvocation:(NSInvocation*)invocation {
  // Called when this proxy is accessed through -asUIScrollView and the method
  // is not implemented in this class. Forwards the invocation to the undelrying
  // scroll view.
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

#pragma mark - KVO

- (void)addObserver:(NSObject*)observer
         forKeyPath:(NSString*)keyPath
            options:(NSKeyValueObservingOptions)options
            context:(nullable void*)context {
  // KVO against CRWWebViewScrollViewProxy works as KVO against the underlying
  // scroll view, except that |object| parameter of the notification points to
  // CRWWebViewScrollViewProxy, not the undelying scroll view. This is achieved
  // by CRWKeyValueObserverForwarder.
  NSMapTable<id, CRWKeyValueObserverForwarder*>* map =
      [_keyValueObserverForwarders objectForKey:keyPath];
  if (!map) {
    map =
        [NSMapTable mapTableWithKeyOptions:NSMapTableObjectPointerPersonality |
                                           NSMapTableWeakMemory
                              valueOptions:NSMapTableStrongMemory];
    [_keyValueObserverForwarders setObject:map forKey:keyPath];
  }
  CRWKeyValueObserverForwarder* observerForwarder = [map objectForKey:observer];
  if (!observerForwarder) {
    observerForwarder =
        [[CRWKeyValueObserverForwarder alloc] initWithWrappedObserver:observer
                                                               object:self];
    [map setObject:observerForwarder forKey:observer];
  }
  ++observerForwarder.observationCount;

  [self.underlyingScrollView addObserver:observerForwarder
                              forKeyPath:keyPath
                                 options:options
                                 context:context];
}

- (void)removeObserver:(NSObject*)observer forKeyPath:(NSString*)keyPath {
  NSMapTable<id, CRWKeyValueObserverForwarder*>* map =
      [_keyValueObserverForwarders objectForKey:keyPath];
  CRWKeyValueObserverForwarder* observerForwarder = [map objectForKey:observer];
  if (!observerForwarder) {
    [NSException raise:NSRangeException
                format:@"Cannot remove an observer %@ for the key path \"%@\" "
                       @"from %@ because it is not registered as an observer.",
                       observer, keyPath, self];
  }

  [self.underlyingScrollView removeObserver:observerForwarder
                                 forKeyPath:keyPath];

  // It is technically allowed to call -addObserver:forKeypath:options:context:
  // multiple times with the same |observer| and same |keyPath|. And you need to
  // call -removeObserver:forKeyPath: the same number of times to remove it.
  --observerForwarder.observationCount;
  if (observerForwarder.observationCount == 0) {
    [map removeObjectForKey:observer];
    if (map.count == 0) {
      [_keyValueObserverForwarders removeObjectForKey:keyPath];
    }
  }
}

@end
