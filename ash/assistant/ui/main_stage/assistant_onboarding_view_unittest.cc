// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_onboarding_view.h"

#include <memory>
#include <string>

#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/test_support/mock_assistant_view_delegate.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

using testing::Return;

// Constants.
constexpr char kPrimaryUserGivenName[] = "Foo";

// AssistantOnboardingViewTest -------------------------------------------------

class AssistantOnboardingViewTest : public AssistantAshTestBase {
 public:
  AssistantOnboardingViewTest() = default;
  ~AssistantOnboardingViewTest() override = default;

  // AssistantAshTestBase:
  void SetUp() override {
    AssistantAshTestBase::SetUp();

    delegate_ = std::make_unique<MockAssistantViewDelegate>();

    ON_CALL(*delegate_, GetPrimaryUserGivenName())
        .WillByDefault(Return(kPrimaryUserGivenName));

    view_ = std::make_unique<AssistantOnboardingView>(delegate_.get());
  }

  AssistantOnboardingView* view() { return view_.get(); }

  views::Label* greeting_label() {
    return static_cast<views::Label*>(view_->children().at(0));
  }

  views::Label* description_label() {
    return static_cast<views::Label*>(view_->children().at(1));
  }

 private:
  std::unique_ptr<MockAssistantViewDelegate> delegate_;
  std::unique_ptr<AssistantOnboardingView> view_;
};

}  // namespace

// Tests -----------------------------------------------------------------------

TEST_F(AssistantOnboardingViewTest, ShouldHaveExpectedGreeting) {
  EXPECT_EQ(greeting_label()->GetText(),
            base::UTF8ToUTF16(
                base::StringPrintf("Good morning %s,", kPrimaryUserGivenName)));
}

TEST_F(AssistantOnboardingViewTest, ShouldHaveExpectedDescription) {
  EXPECT_EQ(description_label()->GetText(),
            base::UTF8ToUTF16(
                "I'm your Google Assistant, here to help you throughout your "
                "day!\nHere are some things you can try to get started."));
}

}  // namespace ash
