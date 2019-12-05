// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

#import <UIKit/UIKit.h>

#include "base/compiler_specific.h"
#import "ios/web/web_state/ui/crw_web_view_scroll_view_delegate_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// TODO(crbug.com/1030168): Rewrite tests Delegate, MultipleScrollView,
// DelegateClearingUp not to depend on this, and delete this.
@interface CRWWebViewScrollViewProxy (Testing)

@property(nonatomic, readonly) CRWWebViewScrollViewDelegateProxy* delegateProxy;

@end

namespace {

class CRWWebViewScrollViewProxyTest : public PlatformTest {
 protected:
  void SetUp() override {
    mockScrollView_ = [OCMockObject niceMockForClass:[UIScrollView class]];
    webViewScrollViewProxy_ = [[CRWWebViewScrollViewProxy alloc] init];
  }
  ~CRWWebViewScrollViewProxyTest() override {
    [webViewScrollViewProxy_ setScrollView:nil];
  }
  id mockScrollView_;
  CRWWebViewScrollViewProxy* webViewScrollViewProxy_;
};

// Tests that the UIScrollViewDelegate is set correctly.
TEST_F(CRWWebViewScrollViewProxyTest, Delegate) {
  [static_cast<UIScrollView*>([mockScrollView_ expect])
      setDelegate:webViewScrollViewProxy_.delegateProxy];
  [webViewScrollViewProxy_ setScrollView:mockScrollView_];
  EXPECT_OCMOCK_VERIFY(mockScrollView_);
}

// Tests that setting 2 scroll views consecutively, clears the delegate of the
// previous scroll view.
TEST_F(CRWWebViewScrollViewProxyTest, MultipleScrollView) {
  UIScrollView* mockScrollView1 = [[UIScrollView alloc] init];
  UIScrollView* mockScrollView2 = [[UIScrollView alloc] init];
  [webViewScrollViewProxy_ setScrollView:mockScrollView1];
  [webViewScrollViewProxy_ setScrollView:mockScrollView2];
  EXPECT_FALSE([mockScrollView1 delegate]);
  EXPECT_EQ(webViewScrollViewProxy_.delegateProxy, [mockScrollView2 delegate]);
  [webViewScrollViewProxy_ setScrollView:nil];
}

// Tests that when releasing a scroll view from the CRWWebViewScrollViewProxy,
// the UIScrollView's delegate is also cleared.
TEST_F(CRWWebViewScrollViewProxyTest, DelegateClearingUp) {
  UIScrollView* mockScrollView1 = [[UIScrollView alloc] init];
  [webViewScrollViewProxy_ setScrollView:mockScrollView1];
  EXPECT_EQ(webViewScrollViewProxy_.delegateProxy, [mockScrollView1 delegate]);
  [webViewScrollViewProxy_ setScrollView:nil];
  EXPECT_FALSE([mockScrollView1 delegate]);
}

// Tests that CRWWebViewScrollViewProxy returns the correct property values from
// the underlying UIScrollView.
TEST_F(CRWWebViewScrollViewProxyTest, ScrollViewPresent) {
  [webViewScrollViewProxy_ setScrollView:mockScrollView_];
  BOOL yes = YES;
  [[[mockScrollView_ stub] andReturnValue:OCMOCK_VALUE(yes)] isZooming];
  EXPECT_TRUE([webViewScrollViewProxy_ isZooming]);

  // Arbitrary point.
  const CGPoint point = CGPointMake(10, 10);
  [[[mockScrollView_ stub] andReturnValue:[NSValue valueWithCGPoint:point]]
      contentOffset];
  EXPECT_TRUE(
      CGPointEqualToPoint(point, [webViewScrollViewProxy_ contentOffset]));

  // Arbitrary inset.
  const UIEdgeInsets contentInset = UIEdgeInsetsMake(10, 10, 10, 10);
  [[[mockScrollView_ stub]
      andReturnValue:[NSValue valueWithUIEdgeInsets:contentInset]]
      contentInset];
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      contentInset, [webViewScrollViewProxy_ contentInset]));

  // Arbitrary inset.
  const UIEdgeInsets scrollIndicatorInsets = UIEdgeInsetsMake(20, 20, 20, 20);
  [[[mockScrollView_ stub]
      andReturnValue:[NSValue valueWithUIEdgeInsets:scrollIndicatorInsets]]
      scrollIndicatorInsets];
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      scrollIndicatorInsets, [webViewScrollViewProxy_ scrollIndicatorInsets]));

  // Arbitrary size.
  const CGSize contentSize = CGSizeMake(19, 19);
  [[[mockScrollView_ stub] andReturnValue:[NSValue valueWithCGSize:contentSize]]
      contentSize];
  EXPECT_TRUE(
      CGSizeEqualToSize(contentSize, [webViewScrollViewProxy_ contentSize]));

  // Arbitrary rect.
  const CGRect frame = CGRectMake(2, 4, 5, 1);
  [[[mockScrollView_ stub] andReturnValue:[NSValue valueWithCGRect:frame]]
      frame];
  EXPECT_TRUE(CGRectEqualToRect(frame, [webViewScrollViewProxy_ frame]));

  [[[mockScrollView_ expect] andReturnValue:@YES] isDecelerating];
  EXPECT_TRUE([webViewScrollViewProxy_ isDecelerating]);

  [[[mockScrollView_ expect] andReturnValue:@NO] isDecelerating];
  EXPECT_FALSE([webViewScrollViewProxy_ isDecelerating]);

  [[[mockScrollView_ expect] andReturnValue:@YES] isDragging];
  EXPECT_TRUE([webViewScrollViewProxy_ isDragging]);

  [[[mockScrollView_ expect] andReturnValue:@NO] isDragging];
  EXPECT_FALSE([webViewScrollViewProxy_ isDragging]);

  [[[mockScrollView_ expect] andReturnValue:@YES] isTracking];
  EXPECT_TRUE([webViewScrollViewProxy_ isTracking]);

  [[[mockScrollView_ expect] andReturnValue:@NO] isTracking];
  EXPECT_FALSE([webViewScrollViewProxy_ isTracking]);

  [[[mockScrollView_ expect] andReturnValue:@YES] scrollsToTop];
  EXPECT_TRUE([webViewScrollViewProxy_ scrollsToTop]);

  [[[mockScrollView_ expect] andReturnValue:@NO] scrollsToTop];
  EXPECT_FALSE([webViewScrollViewProxy_ scrollsToTop]);

  NSArray<__kindof UIView*>* subviews = [NSArray array];
  [[[mockScrollView_ expect] andReturn:subviews] subviews];
  EXPECT_EQ(subviews, [webViewScrollViewProxy_ subviews]);

  [[[mockScrollView_ expect]
      andReturnValue:@(UIScrollViewContentInsetAdjustmentAutomatic)]
      contentInsetAdjustmentBehavior];
  EXPECT_EQ(UIScrollViewContentInsetAdjustmentAutomatic,
            [webViewScrollViewProxy_ contentInsetAdjustmentBehavior]);

  [[[mockScrollView_ expect]
      andReturnValue:@(UIScrollViewContentInsetAdjustmentNever)]
      contentInsetAdjustmentBehavior];
  EXPECT_EQ(UIScrollViewContentInsetAdjustmentNever,
            [webViewScrollViewProxy_ contentInsetAdjustmentBehavior]);
  [[[mockScrollView_ expect] andReturnValue:@(NO)] clipsToBounds];
  EXPECT_FALSE([webViewScrollViewProxy_ clipsToBounds]);
  [[[mockScrollView_ expect] andReturnValue:@(YES)] clipsToBounds];
  EXPECT_TRUE([webViewScrollViewProxy_ clipsToBounds]);
}

// Tests that CRWWebViewScrollViewProxy returns the correct property values when
// there is no underlying UIScrollView.
TEST_F(CRWWebViewScrollViewProxyTest, ScrollViewAbsent) {
  [webViewScrollViewProxy_ setScrollView:nil];

  EXPECT_TRUE(CGPointEqualToPoint(CGPointZero,
                                  [webViewScrollViewProxy_ contentOffset]));
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      UIEdgeInsetsZero, [webViewScrollViewProxy_ contentInset]));
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      UIEdgeInsetsZero, [webViewScrollViewProxy_ scrollIndicatorInsets]));
  EXPECT_TRUE(
      CGSizeEqualToSize(CGSizeZero, [webViewScrollViewProxy_ contentSize]));
  EXPECT_TRUE(CGRectEqualToRect(CGRectZero, [webViewScrollViewProxy_ frame]));
  EXPECT_FALSE([webViewScrollViewProxy_ isDecelerating]);
  EXPECT_FALSE([webViewScrollViewProxy_ isDragging]);
  EXPECT_FALSE([webViewScrollViewProxy_ isTracking]);
  EXPECT_FALSE([webViewScrollViewProxy_ scrollsToTop]);
  EXPECT_EQ((NSUInteger)0, [webViewScrollViewProxy_ subviews].count);
  EXPECT_EQ(UIScrollViewContentInsetAdjustmentAutomatic,
            [webViewScrollViewProxy_ contentInsetAdjustmentBehavior]);
  EXPECT_FALSE([webViewScrollViewProxy_ clipsToBounds]);

  // Make sure setting the properties is fine too.
  // Arbitrary point.
  const CGPoint kPoint = CGPointMake(10, 10);
  [webViewScrollViewProxy_ setContentOffset:kPoint];
  // Arbitrary inset.
  const UIEdgeInsets kContentInset = UIEdgeInsetsMake(10, 10, 10, 10);
  [webViewScrollViewProxy_ setContentInset:kContentInset];
  [webViewScrollViewProxy_ setScrollIndicatorInsets:kContentInset];
  // Arbitrary size.
  const CGSize kContentSize = CGSizeMake(19, 19);
  [webViewScrollViewProxy_ setContentSize:kContentSize];
}

// Tests that CRWWebViewScrollViewProxy returns the correct property values when
// they are set while there isn't an underlying scroll view, then a new scroll
// view is set.
TEST_F(CRWWebViewScrollViewProxyTest, ScrollViewAbsentThenReset) {
  [webViewScrollViewProxy_ setScrollView:nil];
  UIScrollView* scrollView = [[UIScrollView alloc] init];

  [[mockScrollView_ expect] setClipsToBounds:YES];
  [webViewScrollViewProxy_ setClipsToBounds:YES];
  [[mockScrollView_ expect] setContentInsetAdjustmentBehavior:
                                UIScrollViewContentInsetAdjustmentNever];
  [webViewScrollViewProxy_ setContentInsetAdjustmentBehavior:
                               UIScrollViewContentInsetAdjustmentNever];

  [webViewScrollViewProxy_ setScrollView:scrollView];

  [webViewScrollViewProxy_ setScrollView:mockScrollView_];

  EXPECT_OCMOCK_VERIFY(mockScrollView_);
}

// Tests that CRWWebViewScrollViewProxy returns the correct property values when
// they are set while there is an underlying scroll view, then a new scroll view
// is set.
TEST_F(CRWWebViewScrollViewProxyTest, ScrollViewPresentThenReset) {
  [webViewScrollViewProxy_ setScrollView:nil];
  UIScrollView* scrollView = [[UIScrollView alloc] init];

  [webViewScrollViewProxy_ setScrollView:scrollView];
  [[mockScrollView_ expect] setClipsToBounds:YES];
  [webViewScrollViewProxy_ setClipsToBounds:YES];
  [[mockScrollView_ expect] setContentInsetAdjustmentBehavior:
                                UIScrollViewContentInsetAdjustmentNever];
  [webViewScrollViewProxy_ setContentInsetAdjustmentBehavior:
                               UIScrollViewContentInsetAdjustmentNever];

  [webViewScrollViewProxy_ setScrollView:mockScrollView_];

  EXPECT_OCMOCK_VERIFY(mockScrollView_);
}

// Tests releasing a scroll view when none is owned by the
// CRWWebViewScrollViewProxy.
TEST_F(CRWWebViewScrollViewProxyTest, ReleasingAScrollView) {
  [webViewScrollViewProxy_ setScrollView:nil];
}

// Tests that CRWWebViewScrollViewProxy correctly delegates property setters to
// the underlying UIScrollView.
TEST_F(CRWWebViewScrollViewProxyTest, ScrollViewSetProperties) {
  [webViewScrollViewProxy_ setScrollView:mockScrollView_];

  [[mockScrollView_ expect] setContentInsetAdjustmentBehavior:
                                UIScrollViewContentInsetAdjustmentNever];
  [webViewScrollViewProxy_ setContentInsetAdjustmentBehavior:
                               UIScrollViewContentInsetAdjustmentNever];
  [mockScrollView_ verify];
}

// Tests that -setContentInsetAdjustmentBehavior: works even if it is called
// before setting the scroll view.
TEST_F(CRWWebViewScrollViewProxyTest,
       SetContentInsetAdjustmentBehaviorBeforeSettingScrollView) {
  [[mockScrollView_ expect] setContentInsetAdjustmentBehavior:
                                UIScrollViewContentInsetAdjustmentNever];

  [webViewScrollViewProxy_ setScrollView:nil];
  [webViewScrollViewProxy_ setContentInsetAdjustmentBehavior:
                               UIScrollViewContentInsetAdjustmentNever];
  [webViewScrollViewProxy_ setScrollView:mockScrollView_];

  [mockScrollView_ verify];
}

// Tests that -setClipsToBounds: works even if it is called before setting the
// scroll view.
TEST_F(CRWWebViewScrollViewProxyTest, SetClipsToBoundsBeforeSettingScrollView) {
  [[mockScrollView_ expect] setClipsToBounds:YES];

  [webViewScrollViewProxy_ setScrollView:nil];
  [webViewScrollViewProxy_ setClipsToBounds:YES];
  [webViewScrollViewProxy_ setScrollView:mockScrollView_];

  [mockScrollView_ verify];
}

// Tests that frame changes are communicated to observers.
TEST_F(CRWWebViewScrollViewProxyTest, FrameDidChange) {
  UIScrollView* scroll_view = [[UIScrollView alloc] initWithFrame:CGRectZero];
  [webViewScrollViewProxy_ setScrollView:scroll_view];
  id mock_delegate = [OCMockObject
      niceMockForProtocol:@protocol(CRWWebViewScrollViewProxyObserver)];
  [webViewScrollViewProxy_ addObserver:mock_delegate];
  [[mock_delegate expect]
      webViewScrollViewFrameDidChange:webViewScrollViewProxy_];
  scroll_view.frame = CGRectMake(1, 2, 3, 4);
  [mock_delegate verify];
  [webViewScrollViewProxy_ setScrollView:nil];
}

// Tests that contentInset changes are communicated to observers.
TEST_F(CRWWebViewScrollViewProxyTest, ContentInsetDidChange) {
  UIScrollView* scroll_view = [[UIScrollView alloc] initWithFrame:CGRectZero];
  [webViewScrollViewProxy_ setScrollView:scroll_view];
  id mock_delegate = [OCMockObject
      niceMockForProtocol:@protocol(CRWWebViewScrollViewProxyObserver)];
  [webViewScrollViewProxy_ addObserver:mock_delegate];
  [[mock_delegate expect]
      webViewScrollViewDidResetContentInset:webViewScrollViewProxy_];
  scroll_view.contentInset = UIEdgeInsetsMake(0, 1, 2, 3);
  [mock_delegate verify];
  [webViewScrollViewProxy_ setScrollView:nil];
}

// Verifies that method calls to -asUIScrollView are simply forwarded to the
// underlying scroll view if the method is not implemented in
// CRWWebViewScrollViewProxy.
TEST_F(CRWWebViewScrollViewProxyTest, AsUIScrollViewWithUnderlyingScrollView) {
  [webViewScrollViewProxy_ setScrollView:mockScrollView_];

  // Verifies that a return value is properly propagated.
  // -viewPrintFormatter is not implemented in CRWWebViewScrollViewProxy.
  UIViewPrintFormatter* print_formatter_mock =
      OCMClassMock([UIViewPrintFormatter class]);
  OCMStub([mockScrollView_ viewPrintFormatter]).andReturn(print_formatter_mock);
  EXPECT_EQ(print_formatter_mock,
            [[webViewScrollViewProxy_ asUIScrollView] viewPrintFormatter]);

  // Verifies that a parameter is properly propagated.
  // -drawRect: is not implemented in CRWWebViewScrollViewProxy.
  CGRect rect = CGRectMake(0, 0, 1, 1);
  OCMExpect([mockScrollView_ drawRect:rect]);
  [[webViewScrollViewProxy_ asUIScrollView] drawRect:rect];
  EXPECT_OCMOCK_VERIFY((id)mockScrollView_);

  [webViewScrollViewProxy_ setScrollView:nil];
}

// Verifies that method calls to -asUIScrollView are no-op if the underlying
// scroll view is nil and the method is not implemented in
// CRWWebViewScrollViewProxy.
TEST_F(CRWWebViewScrollViewProxyTest,
       AsUIScrollViewWithoutUnderlyingScrollView) {
  [webViewScrollViewProxy_ setScrollView:nil];

  // Any methods should return nil when the underlying scroll view is nil.
  EXPECT_EQ(nil, [[webViewScrollViewProxy_ asUIScrollView] viewPrintFormatter]);

  // It is expected that nothing happens. Just verifies that it doesn't crash.
  CGRect rect = CGRectMake(0, 0, 1, 1);
  [[webViewScrollViewProxy_ asUIScrollView] drawRect:rect];
}

// Verify that -[CRWWebViewScrollViewProxy isKindOfClass:] works as expected.
TEST_F(CRWWebViewScrollViewProxyTest, IsKindOfClass) {
  // The proxy is a kind of its own class.
  EXPECT_TRUE([webViewScrollViewProxy_
      isKindOfClass:[CRWWebViewScrollViewProxy class]]);

  // The proxy prentends itself to be a kind of UIScrollView.
  EXPECT_TRUE([webViewScrollViewProxy_ isKindOfClass:[UIScrollView class]]);

  // It should return YES for ancestor classes of UIScrollView.
  EXPECT_TRUE([webViewScrollViewProxy_ isKindOfClass:[UIView class]]);

  // Returns NO if none of above applies.
  EXPECT_FALSE([webViewScrollViewProxy_ isKindOfClass:[NSString class]]);
}

// Verify that -[CRWWebViewScrollViewProxy respondsToSelector:] works as
// expected.
TEST_F(CRWWebViewScrollViewProxyTest, RespondsToSelector) {
  // A method defined in CRWWebViewScrollViewProxy but not in UIScrollView.
  EXPECT_TRUE(
      [webViewScrollViewProxy_ respondsToSelector:@selector(addObserver:)]);

  // A method defined in CRWWebViewScrollViewProxy and also in UIScrollView.
  EXPECT_TRUE(
      [webViewScrollViewProxy_ respondsToSelector:@selector(contentOffset)]);

  // A method defined in UIScrollView but not in CRWWebViewScrollViewProxy.
  EXPECT_TRUE(
      [webViewScrollViewProxy_ respondsToSelector:@selector(indexDisplayMode)]);

  // A method defined in UIView (a superclass of UIScrollView) but not in
  // CRWWebViewScrollViewProxy.
  EXPECT_TRUE([webViewScrollViewProxy_
      respondsToSelector:@selector(viewPrintFormatter)]);

  // A method defined in none of above.
  EXPECT_FALSE(
      [webViewScrollViewProxy_ respondsToSelector:@selector(containsString:)]);
}

// Tests delegate method forwarding to [webViewScrollViewProxy_
// asUIScrollView].delegate when:
//   - [webViewScrollViewProxy_ asUIScrollView].delegate is not nil
//   - CRWWebViewScrollViewDelegateProxy implements the method
//
// Expects that a method call to the delegate of the underlying scroll view is
// forwarded to [webViewScrollViewProxy_ asUIScrollView].delegate.
TEST_F(CRWWebViewScrollViewProxyTest,
       ProxyDelegateMethodForwardingForImplementedMethod) {
  UIScrollView* underlying_scroll_view = [[UIScrollView alloc] init];
  [webViewScrollViewProxy_ setScrollView:underlying_scroll_view];

  id<UIScrollViewDelegate> mock_proxy_delegate =
      OCMProtocolMock(@protocol(UIScrollViewDelegate));
  [webViewScrollViewProxy_ asUIScrollView].delegate = mock_proxy_delegate;

  UIView* mock_view = OCMClassMock([UIView class]);
  OCMExpect([mock_proxy_delegate
      scrollViewWillBeginZooming:[webViewScrollViewProxy_ asUIScrollView]
                        withView:mock_view]);

  EXPECT_TRUE([underlying_scroll_view.delegate
      respondsToSelector:@selector(scrollViewWillBeginZooming:withView:)]);
  [underlying_scroll_view.delegate
      scrollViewWillBeginZooming:underlying_scroll_view
                        withView:mock_view];

  EXPECT_OCMOCK_VERIFY(static_cast<id>(mock_proxy_delegate));
  [webViewScrollViewProxy_ setScrollView:nil];
}

// Tests delegate method forwarding to [webViewScrollViewProxy_
// asUIScrollView].delegate when:
//   - [webViewScrollViewProxy_ asUIScrollView].delegate is not nil
//   - CRWWebViewScrollViewDelegateProxy does *not* implement the method
//
// Expects that a method call to the delegate of the underlying scroll view is
// forwarded to [webViewScrollViewProxy_ asUIScrollView].delegate.
TEST_F(CRWWebViewScrollViewProxyTest,
       ProxyDelegateMethodForwardingForUnimplementedMethod) {
  UIScrollView* underlying_scroll_view = [[UIScrollView alloc] init];
  [webViewScrollViewProxy_ setScrollView:underlying_scroll_view];

  id<UIScrollViewDelegate> mock_proxy_delegate =
      OCMProtocolMock(@protocol(UIScrollViewDelegate));
  [webViewScrollViewProxy_ asUIScrollView].delegate = mock_proxy_delegate;

  UIView* mock_view = OCMClassMock([UIView class]);
  OCMExpect(
      [mock_proxy_delegate
          viewForZoomingInScrollView:[webViewScrollViewProxy_ asUIScrollView]])
      .andReturn(mock_view);

  EXPECT_TRUE([underlying_scroll_view.delegate
      respondsToSelector:@selector(viewForZoomingInScrollView:)]);
  EXPECT_EQ(mock_view, [underlying_scroll_view.delegate
                           viewForZoomingInScrollView:underlying_scroll_view]);

  EXPECT_OCMOCK_VERIFY(static_cast<id>(mock_proxy_delegate));
  [webViewScrollViewProxy_ setScrollView:nil];
}

// Tests delegate method forwarding to [webViewScrollViewProxy_
// asUIScrollView].delegate when:
//   - [webViewScrollViewProxy_ asUIScrollView].delegate is nil
//   - CRWWebViewScrollViewDelegateProxy implements the method
//
// Expects that the delegate of the underlying scroll view responds to the
// method but does nothing.
TEST_F(CRWWebViewScrollViewProxyTest,
       ProxyDelegateMethodForwardingForImplementedMethodWhenDelegateIsNil) {
  UIScrollView* underlying_scroll_view = [[UIScrollView alloc] init];
  [webViewScrollViewProxy_ setScrollView:underlying_scroll_view];

  [webViewScrollViewProxy_ asUIScrollView].delegate = nil;

  EXPECT_TRUE([underlying_scroll_view.delegate
      respondsToSelector:@selector(scrollViewWillBeginZooming:withView:)]);
  UIView* mock_view = OCMClassMock([UIView class]);
  // Expects that nothing happens by calling this.
  [underlying_scroll_view.delegate
      scrollViewWillBeginZooming:underlying_scroll_view
                        withView:mock_view];

  [webViewScrollViewProxy_ setScrollView:nil];
}

// Tests delegate method forwarding to [webViewScrollViewProxy_
// asUIScrollView].delegate when:
//   - [webViewScrollViewProxy_ asUIScrollView].delegate is nil
//   - CRWWebViewScrollViewDelegateProxy does *not* implement the method
//
// Expects that the delegate of the underlying scroll view does *not* respond to
// the method.
TEST_F(CRWWebViewScrollViewProxyTest,
       ProxyDelegateMethodForwardingForUnimplementedMethodWhenDelegateIsNil) {
  UIScrollView* underlying_scroll_view = [[UIScrollView alloc] init];
  [webViewScrollViewProxy_ setScrollView:underlying_scroll_view];

  [webViewScrollViewProxy_ asUIScrollView].delegate = nil;

  EXPECT_FALSE([underlying_scroll_view.delegate
      respondsToSelector:@selector(viewForZoomingInScrollView:)]);

  [webViewScrollViewProxy_ setScrollView:nil];
}

}  // namespace
