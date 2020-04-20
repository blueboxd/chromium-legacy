// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/translate_infobar_delegate_observer_bridge.h"

#include <memory>

#include "components/translate/core/browser/mock_translate_infobar_delegate.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using translate::testing::MockTranslateInfoBarDelegate;
using translate::testing::MockTranslateInfoBarDelegateFactory;

@interface TestTranslateInfoBarDelegateObserver
    : NSObject <TranslateInfobarDelegateObserving>

@property(nonatomic) BOOL onTranslateStepChangedCalled;

@property(nonatomic) translate::TranslateStep translateStep;

@property(nonatomic) translate::TranslateErrors::Type errorType;

@property(nonatomic) BOOL onDidDismissWithoutInteractionCalled;

@end

@implementation TestTranslateInfoBarDelegateObserver

#pragma mark - TranslateInfobarDelegateObserving

- (void)translateInfoBarDelegate:(translate::TranslateInfoBarDelegate*)delegate
          didChangeTranslateStep:(translate::TranslateStep)step
                   withErrorType:(translate::TranslateErrors::Type)errorType {
  self.onTranslateStepChangedCalled = YES;
  self.translateStep = step;
  self.errorType = errorType;
}

- (BOOL)translateInfoBarDelegateDidDismissWithoutInteraction:
    (translate::TranslateInfoBarDelegate*)delegate {
  self.onDidDismissWithoutInteractionCalled = YES;
  return YES;
}

@end

class TranslateInfobarDelegateObserverBridgeTest : public PlatformTest {
 protected:
  TranslateInfobarDelegateObserverBridgeTest()
      : delegate_factory_("fr", "en"),
        observer_([[TestTranslateInfoBarDelegateObserver alloc] init]) {
    // Make sure the observer bridge sets up itself as an observer.
    EXPECT_CALL(*GetDelegate(), AddObserver(_));

    observer_bridge_ = std::make_unique<TranslateInfobarDelegateObserverBridge>(
        delegate_factory_.GetMockTranslateInfoBarDelegate(), observer_);
  }

  ~TranslateInfobarDelegateObserverBridgeTest() override {
    // Make sure the observer bridge removes itself as an observer.
    EXPECT_CALL(*GetDelegate(), RemoveObserver(_));
  }

  MockTranslateInfoBarDelegate* GetDelegate() {
    return delegate_factory_.GetMockTranslateInfoBarDelegate();
  }

  translate::TranslateInfoBarDelegate::Observer* GetObserverBridge() {
    return observer_bridge_.get();
  }

  TestTranslateInfoBarDelegateObserver* GetDelegateObserver() {
    return observer_;
  }

 private:
  MockTranslateInfoBarDelegateFactory delegate_factory_;
  TestTranslateInfoBarDelegateObserver* observer_;
  std::unique_ptr<TranslateInfobarDelegateObserverBridge> observer_bridge_;

  DISALLOW_COPY_AND_ASSIGN(TranslateInfobarDelegateObserverBridgeTest);
};

// Tests that |OnTranslateStepChanged| call is forwarded by the observer bridge.
TEST_F(TranslateInfobarDelegateObserverBridgeTest, OnTranslateStepChanged) {
  ASSERT_FALSE(GetDelegateObserver().onTranslateStepChangedCalled);
  GetObserverBridge()->OnTranslateStepChanged(
      translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR,
      translate::TranslateErrors::Type::BAD_ORIGIN);
  EXPECT_TRUE(GetDelegateObserver().onTranslateStepChangedCalled);
  EXPECT_EQ(GetDelegateObserver().translateStep,
            translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR);
  EXPECT_EQ(GetDelegateObserver().errorType,
            translate::TranslateErrors::Type::BAD_ORIGIN);
}

// Tests that |IsDeclinedByUser| call is forwarded by the observer bridge.
TEST_F(TranslateInfobarDelegateObserverBridgeTest,
       DidDismissWithoutInteraction) {
  ASSERT_FALSE(GetDelegateObserver().onDidDismissWithoutInteractionCalled);
  GetObserverBridge()->IsDeclinedByUser();
  EXPECT_TRUE(GetDelegateObserver().onDidDismissWithoutInteractionCalled);
}
