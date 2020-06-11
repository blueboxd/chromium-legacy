// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_onboarding_view.h"

#include <memory>
#include <string>

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/test_support/mock_assistant_view_delegate.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

using testing::Return;

// Constants.
constexpr char kPrimaryUserGivenName[] = "Foo";

// ScopedShowUi ----------------------------------------------------------------

class ScopedShowUi {
 public:
  ScopedShowUi()
      : original_visibility_(
            AssistantUiController::Get()->GetModel()->visibility()) {
    AssistantUiController::Get()->ShowUi(
        chromeos::assistant::mojom::AssistantEntryPoint::kUnspecified);
  }

  ScopedShowUi(const ScopedShowUi&) = delete;
  ScopedShowUi& operator=(const ScopedShowUi&) = delete;

  ~ScopedShowUi() {
    switch (original_visibility_) {
      case AssistantVisibility::kClosed:
        AssistantUiController::Get()->CloseUi(
            chromeos::assistant::mojom::AssistantExitPoint::kUnspecified);
        return;
      case AssistantVisibility::kVisible:
        // No action necessary.
        return;
    }
  }

 private:
  const AssistantVisibility original_visibility_;
};

// AssistantOnboardingViewTest -------------------------------------------------

class AssistantOnboardingViewTest : public AssistantAshTestBase {
 public:
  AssistantOnboardingViewTest()
      : AssistantAshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AssistantOnboardingViewTest() override = default;

  // AssistantAshTestBase:
  void SetUp() override {
    AssistantAshTestBase::SetUp();

    delegate_ = std::make_unique<MockAssistantViewDelegate>();

    ON_CALL(*delegate_, GetPrimaryUserGivenName())
        .WillByDefault(Return(kPrimaryUserGivenName));

    view_ = std::make_unique<AssistantOnboardingView>(delegate_.get());
  }

  void AdvanceClock(base::TimeDelta time_delta) {
    task_environment()->AdvanceClock(time_delta);
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
  // Advance clock to midnight tomorrow.
  AdvanceClock(base::Time::Now().LocalMidnight() +
               base::TimeDelta::FromHours(24) - base::Time::Now());

  {
    // Verify 4:59 AM.
    AdvanceClock(base::TimeDelta::FromHours(4) +
                 base::TimeDelta::FromMinutes(59));
    ScopedShowUi scoped_show_ui;
    EXPECT_EQ(greeting_label()->GetText(),
              base::UTF8ToUTF16(
                  base::StringPrintf("Good night %s,", kPrimaryUserGivenName)));
  }

  {
    // Verify 5:00 AM.
    AdvanceClock(base::TimeDelta::FromMinutes(1));
    ScopedShowUi scoped_show_ui;
    EXPECT_EQ(greeting_label()->GetText(),
              base::UTF8ToUTF16(base::StringPrintf("Good morning %s,",
                                                   kPrimaryUserGivenName)));
  }

  {
    // Verify 11:59 AM.
    AdvanceClock(base::TimeDelta::FromHours(6) +
                 base::TimeDelta::FromMinutes(59));
    ScopedShowUi scoped_show_ui;
    EXPECT_EQ(greeting_label()->GetText(),
              base::UTF8ToUTF16(base::StringPrintf("Good morning %s,",
                                                   kPrimaryUserGivenName)));
  }

  {
    // Verify 12:00 PM.
    AdvanceClock(base::TimeDelta::FromMinutes(1));
    ScopedShowUi scoped_show_ui;
    EXPECT_EQ(greeting_label()->GetText(),
              base::UTF8ToUTF16(base::StringPrintf("Good afternoon %s,",
                                                   kPrimaryUserGivenName)));
  }

  {
    // Verify 4:59 PM.
    AdvanceClock(base::TimeDelta::FromHours(4) +
                 base::TimeDelta::FromMinutes(59));
    ScopedShowUi scoped_show_ui;
    EXPECT_EQ(greeting_label()->GetText(),
              base::UTF8ToUTF16(base::StringPrintf("Good afternoon %s,",
                                                   kPrimaryUserGivenName)));
  }

  {
    // Verify 5:00 PM.
    AdvanceClock(base::TimeDelta::FromMinutes(1));
    ScopedShowUi scoped_show_ui;
    EXPECT_EQ(greeting_label()->GetText(),
              base::UTF8ToUTF16(base::StringPrintf("Good evening %s,",
                                                   kPrimaryUserGivenName)));
  }

  {
    // Verify 10:59 PM.
    AdvanceClock(base::TimeDelta::FromHours(5) +
                 base::TimeDelta::FromMinutes(59));
    ScopedShowUi scoped_show_ui;
    EXPECT_EQ(greeting_label()->GetText(),
              base::UTF8ToUTF16(base::StringPrintf("Good evening %s,",
                                                   kPrimaryUserGivenName)));
  }

  {
    // Verify 11:00 PM.
    AdvanceClock(base::TimeDelta::FromMinutes(1));
    ScopedShowUi scoped_show_ui;
    EXPECT_EQ(greeting_label()->GetText(),
              base::UTF8ToUTF16(
                  base::StringPrintf("Good night %s,", kPrimaryUserGivenName)));
  }
}

TEST_F(AssistantOnboardingViewTest, ShouldHaveExpectedIntro) {
  EXPECT_EQ(intro_label()->GetText(),
            base::UTF8ToUTF16(
                "I'm your Google Assistant, here to help you throughout your "
                "day!\nHere are some things you can try to get started."));
}

}  // namespace ash
