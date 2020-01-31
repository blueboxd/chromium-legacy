// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/quick_answers_client.h"

#include <memory>
#include <string>

#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/mojom/assistant_state_controller.mojom.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/constants/chromeos_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace quick_answers {
namespace {

class MockQuickAnswersDelegate
    : public QuickAnswersClient::QuickAnswersDelegate {
 public:
  MockQuickAnswersDelegate() = default;

  MockQuickAnswersDelegate(const MockQuickAnswersDelegate&) = delete;
  MockQuickAnswersDelegate& operator=(const MockQuickAnswersDelegate&) = delete;

  // QuickAnswersClient::QuickAnswersDelegate:
  MOCK_METHOD1(OnQuickAnswerReceived, void(std::unique_ptr<QuickAnswer>));
  MOCK_METHOD1(OnRequestPreprocessFinish, void(const QuickAnswersRequest&));
  MOCK_METHOD1(OnEligibilityChanged, void(bool));
  MOCK_METHOD0(OnNetworkError, void());
};

}  // namespace

class QuickAnswersClientTest : public testing::Test {
 public:
  QuickAnswersClientTest() = default;

  QuickAnswersClientTest(const QuickAnswersClientTest&) = delete;
  QuickAnswersClientTest& operator=(const QuickAnswersClientTest&) = delete;

  void SetUp() override {
    assistant_state_ = std::make_unique<ash::AssistantState>();
    mock_delegate_ = std::make_unique<MockQuickAnswersDelegate>();

    client_ = std::make_unique<QuickAnswersClient>(&test_url_loader_factory_,
                                                   assistant_state_.get(),
                                                   mock_delegate_.get());
  }

  void TearDown() override { client_.reset(); }

 protected:
  void NotifyAssistantStateChange(
      bool setting_enabled,
      bool context_enabled,
      ash::mojom::AssistantAllowedState assistant_state,
      const std::string& locale) {
    client_->OnAssistantSettingsEnabled(setting_enabled);
    client_->OnAssistantContextEnabled(context_enabled);
    client_->OnAssistantFeatureAllowedChanged(assistant_state);
    client_->OnLocaleChanged(locale);
  }

  std::unique_ptr<QuickAnswersClient> client_;
  std::unique_ptr<MockQuickAnswersDelegate> mock_delegate_;
  std::unique_ptr<ash::AssistantState> assistant_state_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(QuickAnswersClientTest, FeatureEligible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({chromeos::features::kQuickAnswers}, {});

  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(true)).Times(1);

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/true,
      /*assistant_state=*/ash::mojom::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, FeatureIneligibleAfterContextDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({chromeos::features::kQuickAnswers}, {});

  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(true));
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false));

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/true,
      /*assistant_state=*/ash::mojom::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/false,
      /*assistant_state=*/ash::mojom::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {chromeos::features::kQuickAnswers});

  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/true,
      /*assistant_state=*/ash::mojom::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, AssistantSettingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {chromeos::features::kQuickAnswers});

  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);

  NotifyAssistantStateChange(
      /*setting_enabled=*/false,
      /*context_enabled=*/true,
      /*assistant_state=*/ash::mojom::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, AssistantContextDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {chromeos::features::kQuickAnswers});

  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/false,
      /*assistant_state=*/ash::mojom::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, AssistantNotAllowed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {chromeos::features::kQuickAnswers});

  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/true,
      /*assistant_state=*/
      ash::mojom::AssistantAllowedState::DISALLOWED_BY_POLICY,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, UnsupportedLocale) {
  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/true,
      /*assistant_state=*/ash::mojom::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-GB");
}

TEST_F(QuickAnswersClientTest, NetworkError) {
  // Verify that OnNetworkError is called.
  EXPECT_CALL(*mock_delegate_, OnNetworkError());
  EXPECT_CALL(*mock_delegate_, OnQuickAnswerReceived(::testing::_)).Times(0);

  client_->OnNetworkError();
}
// TODO(b/144800297): Add more unit tests for sending request.

}  // namespace quick_answers
}  // namespace chromeos
