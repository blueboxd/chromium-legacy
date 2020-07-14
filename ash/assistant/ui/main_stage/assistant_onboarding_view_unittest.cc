// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_onboarding_view.h"

#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/test_support/mock_assistant_view_delegate.h"
#include "ash/assistant/util/test_support/macros.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

using chromeos::assistant::Assistant;
using chromeos::assistant::AssistantInteractionMetadata;
using chromeos::assistant::AssistantInteractionType;
using chromeos::assistant::AssistantQuerySource;
using chromeos::assistant::AssistantSuggestion;
using chromeos::assistant::AssistantSuggestionType;

// Helpers ---------------------------------------------------------------------

AssistantSuggestion CreateSuggestionWithIconUrl(const std::string& icon_url) {
  AssistantSuggestion suggestion;
  suggestion.icon_url = GURL(icon_url);
  return suggestion;
}

template <typename T>
void FindDescendentByClassName(views::View* parent, T** result) {
  DCHECK_EQ(nullptr, *result);
  std::queue<views::View*> children({parent});
  while (!children.empty()) {
    auto* candidate = children.front();
    children.pop();

    if (candidate->GetClassName() == T::kViewClassName) {
      *result = static_cast<T*>(candidate);
      return;
    }

    for (auto* child : candidate->children())
      children.push(child);
  }
}

// Mocks -----------------------------------------------------------------------

class MockAssistantInteractionSubscriber
    : public testing::NiceMock<
          chromeos::assistant::AssistantInteractionSubscriber> {
 public:
  explicit MockAssistantInteractionSubscriber(Assistant* service) {
    scoped_subscriber_.Add(service);
  }

  ~MockAssistantInteractionSubscriber() override = default;

  MOCK_METHOD(void,
              OnInteractionStarted,
              (const AssistantInteractionMetadata&),
              (override));

 private:
  chromeos::assistant::ScopedAssistantInteractionSubscriber scoped_subscriber_{
      this};
};

// ScopedShowUi ----------------------------------------------------------------

class ScopedShowUi {
 public:
  ScopedShowUi()
      : original_visibility_(
            AssistantUiController::Get()->GetModel()->visibility()) {
    AssistantUiController::Get()->ShowUi(
        chromeos::assistant::AssistantEntryPoint::kUnspecified);
  }

  ScopedShowUi(const ScopedShowUi&) = delete;
  ScopedShowUi& operator=(const ScopedShowUi&) = delete;

  ~ScopedShowUi() {
    switch (original_visibility_) {
      case AssistantVisibility::kClosed:
        AssistantUiController::Get()->CloseUi(
            chromeos::assistant::AssistantExitPoint::kUnspecified);
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
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitAndEnableFeature(
        chromeos::assistant::features::kAssistantBetterOnboarding);
  }

  ~AssistantOnboardingViewTest() override = default;

  void AdvanceClock(base::TimeDelta time_delta) {
    task_environment()->AdvanceClock(time_delta);
  }

  void SetOnboardingSuggestions(
      std::vector<AssistantSuggestion> onboarding_suggestions) {
    const_cast<AssistantSuggestionsModel*>(
        AssistantSuggestionsController::Get()->GetModel())
        ->SetOnboardingSuggestions(std::move(onboarding_suggestions));
  }

  views::Label* greeting_label() {
    return static_cast<views::Label*>(onboarding_view()->children().at(0));
  }

  views::Label* intro_label() {
    return static_cast<views::Label*>(onboarding_view()->children().at(1));
  }

  views::View* suggestions_grid() {
    return onboarding_view()->children().at(2);
  }

 private:
  base::test::ScopedRestoreICUDefaultLocale locale_{"en_US"};
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

// Tests -----------------------------------------------------------------------

TEST_F(AssistantOnboardingViewTest, ShouldHaveExpectedGreeting) {
  // Cache the expected given name.
  const std::string given_name = Shell::Get()
                                     ->session_controller()
                                     ->GetPrimaryUserSession()
                                     ->user_info.given_name;

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
                  base::StringPrintf("Good night %s,", given_name.c_str())));
  }

  {
    // Verify 5:00 AM.
    AdvanceClock(base::TimeDelta::FromMinutes(1));
    ScopedShowUi scoped_show_ui;
    EXPECT_EQ(greeting_label()->GetText(),
              base::UTF8ToUTF16(
                  base::StringPrintf("Good morning %s,", given_name.c_str())));
  }

  {
    // Verify 11:59 AM.
    AdvanceClock(base::TimeDelta::FromHours(6) +
                 base::TimeDelta::FromMinutes(59));
    ScopedShowUi scoped_show_ui;
    EXPECT_EQ(greeting_label()->GetText(),
              base::UTF8ToUTF16(
                  base::StringPrintf("Good morning %s,", given_name.c_str())));
  }

  {
    // Verify 12:00 PM.
    AdvanceClock(base::TimeDelta::FromMinutes(1));
    ScopedShowUi scoped_show_ui;
    EXPECT_EQ(greeting_label()->GetText(),
              base::UTF8ToUTF16(base::StringPrintf("Good afternoon %s,",
                                                   given_name.c_str())));
  }

  {
    // Verify 4:59 PM.
    AdvanceClock(base::TimeDelta::FromHours(4) +
                 base::TimeDelta::FromMinutes(59));
    ScopedShowUi scoped_show_ui;
    EXPECT_EQ(greeting_label()->GetText(),
              base::UTF8ToUTF16(base::StringPrintf("Good afternoon %s,",
                                                   given_name.c_str())));
  }

  {
    // Verify 5:00 PM.
    AdvanceClock(base::TimeDelta::FromMinutes(1));
    ScopedShowUi scoped_show_ui;
    EXPECT_EQ(greeting_label()->GetText(),
              base::UTF8ToUTF16(
                  base::StringPrintf("Good evening %s,", given_name.c_str())));
  }

  {
    // Verify 10:59 PM.
    AdvanceClock(base::TimeDelta::FromHours(5) +
                 base::TimeDelta::FromMinutes(59));
    ScopedShowUi scoped_show_ui;
    EXPECT_EQ(greeting_label()->GetText(),
              base::UTF8ToUTF16(
                  base::StringPrintf("Good evening %s,", given_name.c_str())));
  }

  {
    // Verify 11:00 PM.
    AdvanceClock(base::TimeDelta::FromMinutes(1));
    ScopedShowUi scoped_show_ui;
    EXPECT_EQ(greeting_label()->GetText(),
              base::UTF8ToUTF16(
                  base::StringPrintf("Good night %s,", given_name.c_str())));
  }
}

TEST_F(AssistantOnboardingViewTest, ShouldHaveExpectedIntro) {
  ShowAssistantUi();
  EXPECT_EQ(intro_label()->GetText(),
            base::UTF8ToUTF16(
                "I'm your Google Assistant, here to help you throughout your "
                "day!\nHere are some things you can try to get started."));
}

TEST_F(AssistantOnboardingViewTest, ShouldHaveExpectedSuggestions) {
  // Iterate over each onboarding mode.
  for (int mode = 0;
       mode <= static_cast<int>(AssistantOnboardingMode::kMaxValue); ++mode) {
    auto onboarding_mode = static_cast<AssistantOnboardingMode>(mode);
    SetOnboardingMode(onboarding_mode);

    // Determine expected messages based on onboarding mode.
    std::vector<std::string> expected_messages;
    switch (onboarding_mode) {
      case AssistantOnboardingMode::kEducation:
        expected_messages = {
            "Square root of 71", "How far is Venus",     "Set timer",
            "Tell me a joke",    "\"Hello\" in Chinese", "Take a screenshot",
        };
        break;
      case AssistantOnboardingMode::kDefault:
        expected_messages = {
            "5K in miles",    "Population in Nigeria", "Set timer",
            "Tell me a joke", "\"Hello\" in Chinese",  "Take a screenshot",
        };
        break;
    }

    // Show Assistant UI and verify the expected number of suggestion views.
    ShowAssistantUi();
    ASSERT_EQ(suggestions_grid()->children().size(), expected_messages.size());

    // Verify that each suggestion view has the expected message.
    for (size_t i = 0; i < expected_messages.size(); ++i) {
      views::Label* label = nullptr;
      FindDescendentByClassName(suggestions_grid()->children().at(i), &label);
      ASSERT_NE(label, nullptr);
      EXPECT_EQ(label->GetText(), base::UTF8ToUTF16(expected_messages.at(i)));
    }
  }
}

TEST_F(AssistantOnboardingViewTest, ShouldHandleSuggestionPresses) {
  // Show Assistant UI and verify onboarding suggestions exist.
  ShowAssistantUi();
  ASSERT_FALSE(suggestions_grid()->children().empty());

  // Expect a text interaction originating from the onboarding feature...
  MockAssistantInteractionSubscriber subscriber(assistant_service());
  EXPECT_CALL(subscriber, OnInteractionStarted)
      .WillOnce(
          testing::Invoke([](const AssistantInteractionMetadata& metadata) {
            EXPECT_EQ(AssistantInteractionType::kText, metadata.type);
            EXPECT_EQ(AssistantQuerySource::kBetterOnboarding, metadata.source);
          }));

  // ...when an onboarding suggestion is pressed.
  TapOnAndWait(suggestions_grid()->children().at(0));
}

TEST_F(AssistantOnboardingViewTest, ShouldHandleSuggestionUpdates) {
  // Show Assistant UI and verify suggestions exist.
  ShowAssistantUi();
  EXPECT_FALSE(suggestions_grid()->children().empty());

  // Manually create a suggestion.
  AssistantSuggestion suggestion;
  suggestion.id = base::UnguessableToken();
  suggestion.type = AssistantSuggestionType::kBetterOnboarding;
  suggestion.text = "Forced suggestion";

  // Force a model update.
  std::vector<AssistantSuggestion> suggestions;
  suggestions.push_back(std::move(suggestion));
  SetOnboardingSuggestions(std::move(suggestions));

  // Verify view state is updated to reflect model state.
  ASSERT_EQ(suggestions_grid()->children().size(), 1u);
  views::Label* label = nullptr;
  FindDescendentByClassName(suggestions_grid()->children().at(0), &label);
  ASSERT_NE(nullptr, label);
  EXPECT_EQ(label->GetText(), base::UTF8ToUTF16("Forced suggestion"));
}

TEST_F(AssistantOnboardingViewTest, ShouldHandleLocalIcons) {
  MockAssistantViewDelegate delegate;
  EXPECT_CALL(delegate, GetPrimaryUserGivenName)
      .WillOnce(testing::Return("Primary User Given Name"));

  AssistantOnboardingView onboarding_view(&delegate);
  SetOnboardingSuggestions({CreateSuggestionWithIconUrl(
      "googleassistant://resource?type=icon&name=assistant")});

  views::ImageView* icon_view = nullptr;
  FindDescendentByClassName(&onboarding_view, &icon_view);
  ASSERT_NE(nullptr, icon_view);

  const auto& actual = icon_view->GetImage();
  gfx::ImageSkia expected = gfx::CreateVectorIcon(
      gfx::IconDescription(ash::kAssistantIcon, /*size=*/24));

  ASSERT_PIXELS_EQ(actual, expected);
}

TEST_F(AssistantOnboardingViewTest, ShouldHandleRemoteIcons) {
  const gfx::ImageSkia expected =
      gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);

  MockAssistantViewDelegate delegate;
  EXPECT_CALL(delegate, GetPrimaryUserGivenName)
      .WillOnce(testing::Return("Primary User Given Name"));

  AssistantOnboardingView onboarding_view(&delegate);
  EXPECT_CALL(delegate, DownloadImage)
      .WillOnce(testing::Invoke(
          [&](const GURL& url, ImageDownloader::DownloadCallback callback) {
            std::move(callback).Run(expected);
          }));

  SetOnboardingSuggestions({CreateSuggestionWithIconUrl(
      "https://www.gstatic.com/images/branding/product/2x/googleg_48dp.png")});

  views::ImageView* icon_view = nullptr;
  FindDescendentByClassName(&onboarding_view, &icon_view);
  ASSERT_NE(nullptr, icon_view);

  const auto& actual = icon_view->GetImage();
  EXPECT_TRUE(actual.BackedBySameObjectAs(expected));
}

}  // namespace ash
