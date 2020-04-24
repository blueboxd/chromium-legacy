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
#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "chromeos/constants/chromeos_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace quick_answers {

namespace {

class TestResultLoader : public ResultLoader {
 public:
  TestResultLoader(network::mojom::URLLoaderFactory* url_loader_factory,
                   ResultLoaderDelegate* delegate)
      : ResultLoader(url_loader_factory, delegate) {}
  // ResultLoader:
  GURL BuildRequestUrl(const std::string& selected_text) const override {
    return GURL();
  }
  void ProcessResponse(std::unique_ptr<std::string> response_body,
                       ResponseParserCallback complete_callback) override {}
};

class MockResultLoader : public TestResultLoader {
 public:
  MockResultLoader(network::mojom::URLLoaderFactory* url_loader_factory,
                   ResultLoaderDelegate* delegate)
      : TestResultLoader(url_loader_factory, delegate) {}

  MockResultLoader(const MockResultLoader&) = delete;
  MockResultLoader& operator=(const MockResultLoader&) = delete;

  // TestResultLoader:
  MOCK_METHOD1(Fetch, void(const std::string&));
};

}  // namespace

class QuickAnswersClientTest : public testing::Test {
 public:
  QuickAnswersClientTest() = default;

  QuickAnswersClientTest(const QuickAnswersClientTest&) = delete;
  QuickAnswersClientTest& operator=(const QuickAnswersClientTest&) = delete;

  // Testing::Test:
  void SetUp() override {
    assistant_state_ = std::make_unique<ash::AssistantState>();
    mock_delegate_ = std::make_unique<MockQuickAnswersDelegate>();

    client_ = std::make_unique<QuickAnswersClient>(&test_url_loader_factory_,
                                                   assistant_state_.get(),
                                                   mock_delegate_.get());

    result_loader_factory_callback_ = base::BindRepeating(
        &QuickAnswersClientTest::CreateResultLoader, base::Unretained(this));
  }

  void TearDown() override {
    QuickAnswersClient::SetResultLoaderFactoryForTesting(nullptr);
    client_.reset();
  }

 protected:
  void NotifyAssistantStateChange(
      bool setting_enabled,
      bool context_enabled,
      bool quick_answers_enabled,
      ash::mojom::AssistantAllowedState assistant_state,
      const std::string& locale) {
    client_->OnAssistantSettingsEnabled(setting_enabled);
    client_->OnAssistantContextEnabled(context_enabled);
    client_->OnAssistantQuickAnswersEnabled(quick_answers_enabled);
    client_->OnAssistantFeatureAllowedChanged(assistant_state);
    client_->OnLocaleChanged(locale);
  }

  std::unique_ptr<ResultLoader> CreateResultLoader() {
    return std::move(mock_result_loader_);
  }

  std::unique_ptr<QuickAnswersClient> client_;
  std::unique_ptr<MockQuickAnswersDelegate> mock_delegate_;
  std::unique_ptr<MockResultLoader> mock_result_loader_;
  std::unique_ptr<ash::AssistantState> assistant_state_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  QuickAnswersClient::ResultLoaderFactoryCallback
      result_loader_factory_callback_;
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
      /*quick_answers_enabled=*/true,
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
      /*quick_answers_enabled=*/true,
      /*assistant_state=*/ash::mojom::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/false,
      /*quick_answers_enabled=*/true,
      /*assistant_state=*/ash::mojom::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {chromeos::features::kQuickAnswers});

  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(true)).Times(0);

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/true,
      /*quick_answers_enabled=*/true,
      /*assistant_state=*/ash::mojom::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, AssistantSettingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {chromeos::features::kQuickAnswers});

  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(true)).Times(0);

  NotifyAssistantStateChange(
      /*setting_enabled=*/false,
      /*context_enabled=*/true,
      /*quick_answers_enabled=*/true,
      /*assistant_state=*/ash::mojom::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, AssistantContextDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {chromeos::features::kQuickAnswers});

  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(true)).Times(0);

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/false,
      /*quick_answers_enabled=*/true,
      /*assistant_state=*/ash::mojom::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, AssistantNotAllowed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {chromeos::features::kQuickAnswers});

  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(true)).Times(0);

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/true,
      /*quick_answers_enabled=*/true,
      /*assistant_state=*/
      ash::mojom::AssistantAllowedState::DISALLOWED_BY_POLICY,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, UnsupportedLocale) {
  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(true)).Times(0);

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/true,
      /*quick_answers_enabled=*/true,
      /*assistant_state=*/ash::mojom::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-GB");
}

TEST_F(QuickAnswersClientTest, SettingToggleDisabled) {
  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(true)).Times(0);

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/true,
      /*quick_answers_enabled=*/false,
      /*assistant_state=*/ash::mojom::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, NetworkError) {
  // Verify that OnNetworkError is called.
  EXPECT_CALL(*mock_delegate_, OnNetworkError());
  EXPECT_CALL(*mock_delegate_, OnQuickAnswerReceived(::testing::_)).Times(0);

  client_->OnNetworkError();
}

TEST_F(QuickAnswersClientTest, SendRequest) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "sel";

  mock_result_loader_ =
      std::make_unique<MockResultLoader>(&test_url_loader_factory_, nullptr);
  EXPECT_CALL(*mock_result_loader_, Fetch(::testing::Eq("sel")));
  QuickAnswersClient::SetResultLoaderFactoryForTesting(
      &result_loader_factory_callback_);
  EXPECT_CALL(*mock_delegate_,
              OnRequestPreprocessFinish(
                  QuickAnswersRequestEqual(*quick_answers_request)));
  client_->SendRequest(*quick_answers_request);

  std::unique_ptr<QuickAnswer> quick_answer = std::make_unique<QuickAnswer>();
  quick_answer->primary_answer = "answer";
  EXPECT_CALL(*mock_delegate_,
              OnQuickAnswerReceived(QuickAnswerEqual(&(*quick_answer))));
  client_->OnQuickAnswerReceived(std::move(quick_answer));
}

}  // namespace quick_answers
}  // namespace chromeos
