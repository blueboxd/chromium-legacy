// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/quick_answers_controller_impl.h"

#include "ash/quick_answers/quick_answers_ui_controller.h"
#include "ash/quick_answers/ui/quick_answers_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "chromeos/components/quick_answers/quick_answers_consents.h"
#include "chromeos/constants/chromeos_features.h"
#include "services/network/test/test_url_loader_factory.h"

namespace ash {

namespace {

constexpr gfx::Rect kDefaultAnchorBoundsInScreen =
    gfx::Rect(gfx::Point(500, 250), gfx::Size(80, 140));
constexpr char kDefaultTitle[] = "default_title";

}  // namespace

class QuickAnswersControllerTest : public AshTestBase {
 protected:
  QuickAnswersControllerTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kQuickAnswers,
         chromeos::features::kQuickAnswersRichUi},
        {});
  }
  QuickAnswersControllerTest(const QuickAnswersControllerTest&) = delete;
  QuickAnswersControllerTest& operator=(const QuickAnswersControllerTest&) =
      delete;
  ~QuickAnswersControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    controller()->SetClient(
        std::make_unique<chromeos::quick_answers::QuickAnswersClient>(
            &test_url_loader_factory_, ash::AssistantState::Get(),
            controller()->GetQuickAnswersDelegate()));
  }

  QuickAnswersControllerImpl* controller() {
    return static_cast<QuickAnswersControllerImpl*>(
        QuickAnswersController::Get());
  }

  QuickAnswersUiController* ui_controller() {
    return controller()->quick_answers_ui_controller();
  }

  chromeos::quick_answers::QuickAnswersConsent* consent_controller() {
    return controller()->GetConsentControllerForTesting();
  }

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(QuickAnswersControllerTest, ShouldNotShowWhenFeatureNotEligible) {
  controller()->OnEligibilityChanged(false);
  controller()->SetVisibilityForTesting(QuickAnswersVisibility::kPending);
  controller()->MaybeShowQuickAnswers(kDefaultAnchorBoundsInScreen,
                                      kDefaultTitle, {});

  // The feature is not eligible, nothing should be shown.
  EXPECT_FALSE(ui_controller()->is_showing_user_consent_view());
  EXPECT_FALSE(ui_controller()->is_showing_quick_answers_view());
}

TEST_F(QuickAnswersControllerTest, ShouldNotShowWhenClosed) {
  controller()->OnEligibilityChanged(true);
  controller()->SetVisibilityForTesting(QuickAnswersVisibility::kClosed);
  controller()->MaybeShowQuickAnswers(kDefaultAnchorBoundsInScreen,
                                      kDefaultTitle, {});

  // The UI is closed and session is inactive, nothing should be shown.
  EXPECT_FALSE(ui_controller()->is_showing_user_consent_view());
  EXPECT_FALSE(ui_controller()->is_showing_quick_answers_view());
  EXPECT_EQ(controller()->visibility(), QuickAnswersVisibility::kClosed);
}

TEST_F(QuickAnswersControllerTest, AcceptUserConsent) {
  controller()->OnEligibilityChanged(true);
  controller()->SetVisibilityForTesting(QuickAnswersVisibility::kPending);
  controller()->MaybeShowQuickAnswers(kDefaultAnchorBoundsInScreen,
                                      kDefaultTitle, {});
  // Without user consent, only the user consent view should show.
  EXPECT_TRUE(ui_controller()->is_showing_user_consent_view());
  EXPECT_FALSE(ui_controller()->is_showing_quick_answers_view());

  controller()->OnUserConsentGranted();

  // With user consent granted, the consent view should dismiss and the cached
  // quick answer query should show.
  EXPECT_FALSE(ui_controller()->is_showing_user_consent_view());
  EXPECT_TRUE(ui_controller()->is_showing_quick_answers_view());
  EXPECT_EQ(controller()->visibility(), QuickAnswersVisibility::kVisible);
}

TEST_F(QuickAnswersControllerTest, UserConsentAlreadyAccpeted) {
  consent_controller()->StartConsent();
  controller()->OnEligibilityChanged(true);
  controller()->SetVisibilityForTesting(QuickAnswersVisibility::kPending);
  consent_controller()->AcceptConsent(
      chromeos::quick_answers::ConsentInteractionType::kAccept);
  controller()->MaybeShowQuickAnswers(kDefaultAnchorBoundsInScreen,
                                      kDefaultTitle, {});

  // With user consent already accepted, only the quick answers view should
  // show.
  EXPECT_FALSE(ui_controller()->is_showing_user_consent_view());
  EXPECT_TRUE(ui_controller()->is_showing_quick_answers_view());
  EXPECT_EQ(controller()->visibility(), QuickAnswersVisibility::kVisible);
}

TEST_F(QuickAnswersControllerTest, DismissUserConsentView) {
  controller()->OnEligibilityChanged(true);
  controller()->SetVisibilityForTesting(QuickAnswersVisibility::kPending);
  controller()->MaybeShowQuickAnswers(kDefaultAnchorBoundsInScreen,
                                      kDefaultTitle, {});
  EXPECT_TRUE(ui_controller()->is_showing_user_consent_view());

  controller()->DismissQuickAnswers(true);
  EXPECT_FALSE(ui_controller()->is_showing_user_consent_view());
  EXPECT_EQ(controller()->visibility(), QuickAnswersVisibility::kClosed);
}

TEST_F(QuickAnswersControllerTest, DismissQuickAnswersView) {
  consent_controller()->StartConsent();
  controller()->OnEligibilityChanged(true);
  controller()->SetVisibilityForTesting(QuickAnswersVisibility::kPending);
  consent_controller()->AcceptConsent(
      chromeos::quick_answers::ConsentInteractionType::kAccept);
  controller()->MaybeShowQuickAnswers(kDefaultAnchorBoundsInScreen,
                                      kDefaultTitle, {});
  EXPECT_TRUE(ui_controller()->is_showing_quick_answers_view());

  controller()->DismissQuickAnswers(true);
  EXPECT_FALSE(ui_controller()->is_showing_quick_answers_view());
  EXPECT_EQ(controller()->visibility(), QuickAnswersVisibility::kClosed);
}

}  // namespace ash
