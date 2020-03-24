// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/understanding/intent_generator.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/language_detector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace quick_answers {
namespace {

class MockLanguageDetector : public LanguageDetector {
 public:
  MockLanguageDetector() = default;

  MockLanguageDetector(const MockLanguageDetector&) = delete;
  MockLanguageDetector& operator=(const MockLanguageDetector&) = delete;

  ~MockLanguageDetector() override = default;

  // TestResultLoader:
  MOCK_METHOD1(DetectLanguage, std::string(const std::string&));
};

}  // namespace

class IntentGeneratorTest : public testing::Test {
 public:
  IntentGeneratorTest() = default;

  IntentGeneratorTest(const IntentGeneratorTest&) = delete;
  IntentGeneratorTest& operator=(const IntentGeneratorTest&) = delete;

  void SetUp() override {
    intent_generator_ = std::make_unique<IntentGenerator>(
        base::BindOnce(&IntentGeneratorTest::IntentGeneratorTestCallback,
                       base::Unretained(this)));
    // Mock language detector.
    mock_language_detector_ = std::make_unique<MockLanguageDetector>();
    EXPECT_CALL(*mock_language_detector_, DetectLanguage(::testing::_))
        .WillRepeatedly(::testing::Return("en"));
    intent_generator_->SetLanguageDetectorForTesting(
        std::move(mock_language_detector_));
  }

  void TearDown() override { intent_generator_.reset(); }

  void IntentGeneratorTestCallback(const std::string& text, IntentType type) {
    intent_text_ = text;
    intent_type_ = type;
  }

 protected:
  std::unique_ptr<IntentGenerator> intent_generator_;
  std::unique_ptr<MockLanguageDetector> mock_language_detector_;
  std::string intent_text_;
  IntentType intent_type_ = IntentType::kUnknown;
};

TEST_F(IntentGeneratorTest, TranslationIntent) {
  QuickAnswersRequest request;
  request.selected_text = "quick answers";
  request.device_properties.language = "es";
  intent_generator_->GenerateIntent(request);

  EXPECT_EQ(IntentType::kTranslation, intent_type_);
  EXPECT_EQ("quick answers", intent_text_);
}

TEST_F(IntentGeneratorTest, TranslationIntentSameLanguage) {
  QuickAnswersRequest request;
  request.selected_text = "quick answers";
  request.device_properties.language = "en";
  intent_generator_->GenerateIntent(request);

  EXPECT_EQ(IntentType::kUnknown, intent_type_);
  EXPECT_EQ("quick answers", intent_text_);
}

TEST_F(IntentGeneratorTest, TranslationIntentNotEnabled) {
  QuickAnswersRequest request;
  request.selected_text = "quick answers";
  intent_generator_->GenerateIntent(request);

  EXPECT_EQ(IntentType::kUnknown, intent_type_);
  EXPECT_EQ("quick answers", intent_text_);
}

}  // namespace quick_answers
}  // namespace chromeos
