// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/text_fragment_utils.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "ios/web/common/features.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

typedef PlatformTest TextFragmentUtilsTest;

TEST_F(TextFragmentUtilsTest, AreTextFragmentsAllowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kScrollToTextIOS);

  std::unique_ptr<TestWebState> web_state = std::make_unique<TestWebState>();
  TestWebState* web_state_ptr = web_state.get();
  FakeNavigationContext context;
  context.SetWebState(std::move(web_state));

  // Working case: no opener, has user gesture, not same document
  web_state_ptr->SetHasOpener(false);
  context.SetHasUserGesture(true);
  context.SetIsSameDocument(false);
  EXPECT_TRUE(AreTextFragmentsAllowed(&context));

  // Blocking case #1: WebState has an opener
  web_state_ptr->SetHasOpener(true);
  context.SetHasUserGesture(true);
  context.SetIsSameDocument(false);
  EXPECT_FALSE(AreTextFragmentsAllowed(&context));

  // Blocking case #2: No user gesture
  web_state_ptr->SetHasOpener(false);
  context.SetHasUserGesture(false);
  context.SetIsSameDocument(false);
  EXPECT_FALSE(AreTextFragmentsAllowed(&context));

  // Blocking case #3: Same-document navigation
  web_state_ptr->SetHasOpener(false);
  context.SetHasUserGesture(true);
  context.SetIsSameDocument(true);
  EXPECT_FALSE(AreTextFragmentsAllowed(&context));
}

}  // namespace web
