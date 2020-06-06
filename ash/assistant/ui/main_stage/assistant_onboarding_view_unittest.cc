// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_onboarding_view.h"

#include <memory>
#include <string>

#include "ash/assistant/ui/test_support/mock_assistant_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

using testing::Return;

// Constants.
constexpr char kPrimaryUserGivenName[] = "Foo";

// AssistantOnboardingViewTest -------------------------------------------------

class AssistantOnboardingViewTest : public AshTestBase {
 public:
  AssistantOnboardingViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AssistantOnboardingViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    delegate_ = std::make_unique<MockAssistantViewDelegate>();

    ON_CALL(*delegate_, GetPrimaryUserGivenName())
        .WillByDefault(Return(kPrimaryUserGivenName));

    RecreateView();
  }

  void AdvanceClock(base::TimeDelta time_delta) {
    task_environment()->AdvanceClock(time_delta);
  }

  void RecreateView() {
    view_ = std::make_unique<AssistantOnboardingView>(delegate_.get());
  }

  AssistantOnboardingView* view() { return view_.get(); }

  views::Label* greeting_label() {
    return static_cast<views::Label*>(view_->children().at(0));
  }

  views::Label* intro_label() {
    return static_cast<views::Label*>(view_->children().at(1));
  }

 private:
  base::test::ScopedRestoreICUDefaultLocale locale{"en_US"};
  std::unique_ptr<MockAssistantViewDelegate> delegate_;
  std::unique_ptr<AssistantOnboardingView> view_;
};

}  // namespace

// Tests -----------------------------------------------------------------------

TEST_F(AssistantOnboardingViewTest, ShouldHaveExpectedGreeting) {
  // Advance clock to midnight.
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);
  AdvanceClock(base::TimeDelta::FromHours((24 - now.hour) % 24) -
               base::TimeDelta::FromMinutes(now.minute) -
               base::TimeDelta::FromSeconds(now.second) -
               base::TimeDelta::FromMilliseconds(now.millisecond));

  // Verify 4:59 AM.
  AdvanceClock(base::TimeDelta::FromHours(4) +
               base::TimeDelta::FromMinutes(59));
  RecreateView();
  EXPECT_EQ(greeting_label()->GetText(),
            base::UTF8ToUTF16(
                base::StringPrintf("Good night %s,", kPrimaryUserGivenName)));

  // Verify 5:00 AM.
  AdvanceClock(base::TimeDelta::FromMinutes(1));
  RecreateView();
  EXPECT_EQ(greeting_label()->GetText(),
            base::UTF8ToUTF16(
                base::StringPrintf("Good morning %s,", kPrimaryUserGivenName)));

  // Verify 11:59 AM.
  AdvanceClock(base::TimeDelta::FromHours(6) +
               base::TimeDelta::FromMinutes(59));
  RecreateView();
  EXPECT_EQ(greeting_label()->GetText(),
            base::UTF8ToUTF16(
                base::StringPrintf("Good morning %s,", kPrimaryUserGivenName)));

  // Verify 12:00 PM.
  AdvanceClock(base::TimeDelta::FromMinutes(1));
  RecreateView();
  EXPECT_EQ(greeting_label()->GetText(),
            base::UTF8ToUTF16(base::StringPrintf("Good afternoon %s,",
                                                 kPrimaryUserGivenName)));

  // Verify 4:59 PM.
  AdvanceClock(base::TimeDelta::FromHours(4) +
               base::TimeDelta::FromMinutes(59));
  RecreateView();
  EXPECT_EQ(greeting_label()->GetText(),
            base::UTF8ToUTF16(base::StringPrintf("Good afternoon %s,",
                                                 kPrimaryUserGivenName)));

  // Verify 5:00 PM.
  AdvanceClock(base::TimeDelta::FromMinutes(1));
  RecreateView();
  EXPECT_EQ(greeting_label()->GetText(),
            base::UTF8ToUTF16(
                base::StringPrintf("Good evening %s,", kPrimaryUserGivenName)));

  // Verify 10:59 PM.
  AdvanceClock(base::TimeDelta::FromHours(5) +
               base::TimeDelta::FromMinutes(59));
  RecreateView();
  EXPECT_EQ(greeting_label()->GetText(),
            base::UTF8ToUTF16(
                base::StringPrintf("Good evening %s,", kPrimaryUserGivenName)));

  // Verify 11:00 PM.
  AdvanceClock(base::TimeDelta::FromMinutes(1));
  RecreateView();
  EXPECT_EQ(greeting_label()->GetText(),
            base::UTF8ToUTF16(
                base::StringPrintf("Good night %s,", kPrimaryUserGivenName)));
}

TEST_F(AssistantOnboardingViewTest, ShouldHaveExpectedIntro) {
  EXPECT_EQ(intro_label()->GetText(),
            base::UTF8ToUTF16(
                "I'm your Google Assistant, here to help you throughout your "
                "day!\nHere are some things you can try to get started."));
}

}  // namespace ash
