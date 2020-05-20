// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/understanding/intent_generator.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/language_detector.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/text_classifier.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace quick_answers {
namespace {

using chromeos::machine_learning::FakeServiceConnectionImpl;
using machine_learning::mojom::TextAnnotation;
using machine_learning::mojom::TextAnnotationPtr;
using machine_learning::mojom::TextEntity;
using machine_learning::mojom::TextEntityData;
using machine_learning::mojom::TextEntityPtr;

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

    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kQuickAnswersTextAnnotator);
  }

  void TearDown() override { intent_generator_.reset(); }

  void IntentGeneratorTestCallback(const std::string& text, IntentType type) {
    intent_text_ = text;
    intent_type_ = type;
  }

 protected:
  void UseFakeServiceConnection(
      const std::vector<TextAnnotationPtr>& annotations =
          std::vector<TextAnnotationPtr>()) {
    chromeos::machine_learning::ServiceConnection::
        UseFakeServiceConnectionForTesting(&fake_service_connection_);
    fake_service_connection_.SetOutputAnnotation(annotations);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<IntentGenerator> intent_generator_;
  std::unique_ptr<MockLanguageDetector> mock_language_detector_;
  std::string intent_text_;
  IntentType intent_type_ = IntentType::kUnknown;
  base::test::ScopedFeatureList scoped_feature_list_;
  chromeos::machine_learning::FakeServiceConnectionImpl
      fake_service_connection_;
};

TEST_F(IntentGeneratorTest, TranslationIntent) {
  UseFakeServiceConnection();

  QuickAnswersRequest request;
  request.selected_text = "quick answers";
  request.context.device_properties.language = "es";
  intent_generator_->GenerateIntent(request);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kTranslation, intent_type_);
  EXPECT_EQ("quick answers", intent_text_);
}

TEST_F(IntentGeneratorTest, TranslationIntentSameLanguage) {
  UseFakeServiceConnection();

  QuickAnswersRequest request;
  request.selected_text = "quick answers";
  request.context.device_properties.language = "en";
  intent_generator_->GenerateIntent(request);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kUnknown, intent_type_);
  EXPECT_EQ("quick answers", intent_text_);
}

TEST_F(IntentGeneratorTest, TranslationIntentTextLengthAboveThreshold) {
  UseFakeServiceConnection();

  QuickAnswersRequest request;
  request.selected_text =
      "Search the world's information, including webpages, images, videos and "
      "more.";
  request.context.device_properties.language = "es";
  intent_generator_->GenerateIntent(request);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kUnknown, intent_type_);
  EXPECT_EQ(
      "Search the world's information, including webpages, images, videos and "
      "more.",
      intent_text_);
}

TEST_F(IntentGeneratorTest, TranslationIntentNotEnabled) {
  UseFakeServiceConnection();

  QuickAnswersRequest request;
  request.selected_text = "quick answers";
  intent_generator_->GenerateIntent(request);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kUnknown, intent_type_);
  EXPECT_EQ("quick answers", intent_text_);
}

TEST_F(IntentGeneratorTest, TextAnnotationDefinitionIntent) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "unfathomable";

  // Create the test annotations.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(
      TextEntity::New("dictionary",             // Entity name.
                      1.0,                      // Confidence score.
                      TextEntityData::New()));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(0,   // Start offset.
                                                   12,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());

  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kDictionary, intent_type_);
  EXPECT_EQ("unfathomable", intent_text_);
}

TEST_F(IntentGeneratorTest, TextAnnotationDefinitionIntentExtraChars) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "the unfathomable";

  // Create the test annotations.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(
      TextEntity::New("dictionary",             // Entity name.
                      1.0,                      // Confidence score.
                      TextEntityData::New()));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(4,   // Start offset.
                                                   16,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());

  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kUnknown, intent_type_);
  EXPECT_EQ("unfathomable", intent_text_);
}

TEST_F(IntentGeneratorTest, TextAnnotationUnitIntentExtraChars) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "23 cm to";

  // Create the test annotations.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(
      TextEntity::New("unit",                   // Entity name.
                      1.0,                      // Confidence score.
                      TextEntityData::New()));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(0,  // Start offset.
                                                   5,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());

  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kUnit, intent_type_);
  EXPECT_EQ("23 cm", intent_text_);
}

TEST_F(IntentGeneratorTest, TextAnnotationUnitIntentExtraCharsAboveThreshold) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "23 cm is equal to 9.06 inches";

  // Create the test annotations.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(
      TextEntity::New("unit",                   // Entity name.
                      1.0,                      // Confidence score.
                      TextEntityData::New()));  // Data extracted.

  auto dictionary_annotation = TextAnnotation::New(0,  // Start offset.
                                                   5,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());

  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kUnknown, intent_type_);
  EXPECT_EQ("23 cm", intent_text_);
}

TEST_F(IntentGeneratorTest, TextAnnotationIntentNoAnnotation) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "the unfathomable reaches of space";

  std::vector<TextAnnotationPtr> annotations;
  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kUnknown, intent_type_);
  EXPECT_EQ("the unfathomable reaches of space", intent_text_);
}

TEST_F(IntentGeneratorTest, TextAnnotationIntentNoEntity) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "the unfathomable reaches of space";

  std::vector<TextEntityPtr> entities;
  auto dictionary_annotation = TextAnnotation::New(4,   // Start offset.
                                                   16,  // End offset.
                                                   std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(dictionary_annotation->Clone());
  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kUnknown, intent_type_);
  EXPECT_EQ("the unfathomable reaches of space", intent_text_);
}

TEST_F(IntentGeneratorTest, TextAnnotationIntentUnSupportedEntity) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "the unfathomable reaches of space";

  // Create the test annotations.
  std::vector<TextEntityPtr> entities;
  entities.emplace_back(
      TextEntity::New("something_else",         // Entity name.
                      1.0,                      // Confidence score.
                      TextEntityData::New()));  // Data extracted.

  auto some_annotation = TextAnnotation::New(4,   // Start offset.
                                             16,  // End offset.
                                             std::move(entities));

  std::vector<TextAnnotationPtr> annotations;
  annotations.push_back(some_annotation->Clone());
  UseFakeServiceConnection(annotations);

  intent_generator_->GenerateIntent(*quick_answers_request);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(IntentType::kUnknown, intent_type_);
  EXPECT_EQ("the unfathomable reaches of space", intent_text_);
}
}  // namespace quick_answers
}  // namespace chromeos
