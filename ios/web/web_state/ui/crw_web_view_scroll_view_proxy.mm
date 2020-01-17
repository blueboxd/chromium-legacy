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
#import "ios/web/web_state/ui/crw_properties_store.h"
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

  // A storage used to preserve values of the scroll view properties on
  // resetting the underlying scroll view.
  CRWPropertiesStore* _propertiesStore;
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

    if (base::FeatureList::IsEnabled(
            web::features::kPreserveScrollViewProperties)) {
      _propertiesStore = [[CRWPropertiesStore alloc] init];

      // A list of properties preserved on resetting the underlying scroll view.
      //
      // The underlying scroll view can be nil or can be reassigned. Properties
      // of the underlying scroll view are usually not preserved when the scroll
      // view is reassigned. Properties listed here will be preserved i.e.:
      //   - If the property is assigned while the underlying scroll view is
      //   nil,
      //     the assignment is applied when the underlying scroll view is
      //     assigned.
      //   - The property is preserved when the underlying scroll view is
      //     reassigned.
      //
      // This list should contain all properties of UIScrollView and its
      // ancestor classes (not limited to properties explicitly declared in
      // CRWWebViewScrollViewProxy) which:
      //   - is a readwrite property
      //   - AND is supposed to be modified directly, considering it's a scroll
      //     view of a web view. e.g., |frame| and |subviews| do not meet this
      //     condition because they are managed by the web view.
      //
      // TODO(crbug.com/1023250): Make this list comprehensive.
      [_propertiesStore
          registerNonObjectPropertyWithGetter:@selector
          (isDirectionalLockEnabled)
                                       setter:@selector
                                       (setDirectionalLockEnabled:)
                                         size:sizeof(BOOL)];
      [_propertiesStore
          registerObjectPropertyWithGetter:@selector(tintColor)
                                    setter:@selector(setTintColor:)
                                 attribute:CRWStoredPropertyAttributeStrong];
      [_propertiesStore
          registerObjectPropertyWithGetter:@selector(backgroundColor)
                                    setter:@selector(setBackgroundColor:)
                                 attribute:CRWStoredPropertyAttributeCopy];
    }
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

  // Clean up the delegate/observers of the old scroll view, and save its
  // properties for later restoration.
  [self.underlyingScrollView setDelegate:nil];
  [self stopObservingScrollView:self.underlyingScrollView];
  if (base::FeatureList::IsEnabled(
          web::features::kPreserveScrollViewProperties) &&
      self.underlyingScrollView) {
    [_propertiesStore savePropertiesFromObject:self.underlyingScrollView];
  }

  // Set up the delegate/observers of the new scroll view, and restore its
  // properties.
  DCHECK(!scrollView.delegate);
  scrollView.delegate = self.delegateProxy;
  [self startObservingScrollView:scrollView];
  if (base::FeatureList::IsEnabled(
          web::features::kPreserveScrollViewProperties) &&
      scrollView) {
    [_propertiesStore loadPropertiesToObject:scrollView];
    // Clear the stored values of the properties. This prevents from keeping
    // retaining old property values.
    [_propertiesStore clearValues];
  }

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
  // is not implemented in this class.
  if (self.underlyingScrollView) {
    // Forwards the invocation to the undelrying scroll view.
    [invocation invokeWithTarget:self.underlyingScrollView];
  } else {
    BOOL handled = NO;
    if (base::FeatureList::IsEnabled(
            web::features::kPreserveScrollViewProperties)) {
      // Forwards the invocation to the property store. If it is an invocation
      // of a getter or setter of a preserved property, it gets or sets the
      // property in the property store, which is later restored in the
      // underlying scroll view.
      handled =
          [_propertiesStore forwardInvocationToPropertiesStore:invocation];
    }

    // If it is not an invocation of a getter or setter of a preserved property,
    // do nothing and return a zero value. This must be done explicitly because
    // it looks not guaranteed to return a zero value if it does nothing here or
    // call [invocation invokeWithTarget:nil].
    if (!handled && invocation.methodSignature.methodReturnLength > 0) {
      // NSMutableData is initialized with zero bytes.
      NSMutableData* zeroData = [NSMutableData
          dataWithLength:invocation.methodSignature.methodReturnLength];
      [invocation setReturnValue:zeroData.mutableBytes];
    }
  }
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
