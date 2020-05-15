// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/understanding/intent_generator.h"

#include <map>

#include "base/no_destructor.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/language_detector.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"

namespace chromeos {
namespace quick_answers {
namespace {

using chromeos::machine_learning::mojom::LoadModelResult;
using machine_learning::mojom::TextAnnotationPtr;
using machine_learning::mojom::TextAnnotationRequestPtr;
using machine_learning::mojom::TextClassifier;

// TODO(llin): Finalize on the threshold based on user feedback.
constexpr int kUnitConversionIntentAndSelectionLengthDiffThreshold = 5;
constexpr int kTranslationTextLengthThreshold = 50;
constexpr int kDefinitionIntentAndSelectionLengthDiffThreshold = 0;

const std::map<std::string, IntentType>& GetIntentTypeMap() {
  static base::NoDestructor<std::map<std::string, IntentType>> kIntentTypeMap(
      {{"unit", IntentType::kUnit}, {"dictionary", IntentType::kDictionary}});
  return *kIntentTypeMap;
}

bool ExtractEntity(const std::string& selected_text,
                   const std::vector<TextAnnotationPtr>& annotations,
                   std::string* entity_str,
                   std::string* type) {
  for (auto& annotation : annotations) {
    *entity_str =
        selected_text.substr(annotation->start_offset,
                             annotation->end_offset - annotation->start_offset);

    // Use the first entity type.
    auto intent_type_map = GetIntentTypeMap();
    for (const auto& entity : annotation->entities) {
      if (intent_type_map.find(entity->name) != intent_type_map.end()) {
        *type = entity->name;
        return true;
      }
    }
  }

  return false;
}

IntentType RewriteIntent(const std::string& selected_text,
                         const std::string& entity_str,
                         const IntentType intent) {
  int intent_and_selection_length_diff =
      selected_text.length() - entity_str.length();

  if ((intent == IntentType::kUnit &&
       intent_and_selection_length_diff >
           kUnitConversionIntentAndSelectionLengthDiffThreshold) ||
      (intent == IntentType::kDictionary &&
       intent_and_selection_length_diff >
           kDefinitionIntentAndSelectionLengthDiffThreshold)) {
    // Override intent type to |kUnknown| if length diff between intent
    // text and selection text is above the threshold.
    return IntentType::kUnknown;
  }

  return intent;
}

}  // namespace

IntentGenerator::IntentGenerator(IntentGeneratorCallback complete_callback)
    : complete_callback_(std::move(complete_callback)) {
  language_detector_ = std::make_unique<LanguageDetector>();
}

IntentGenerator::~IntentGenerator() {
  if (complete_callback_)
    std::move(complete_callback_).Run(std::string(), IntentType::kUnknown);
}

void IntentGenerator::GenerateIntent(const QuickAnswersRequest& request) {
  if (!features::IsQuickAnswersTextAnnotatorEnabled()) {
    std::move(complete_callback_)
        .Run(request.selected_text, IntentType::kUnknown);
    return;
  }

  // Load text classifier.
  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->LoadTextClassifier(text_classifier_.BindNewPipeAndPassReceiver(),
                           base::BindOnce(&IntentGenerator::LoadModelCallback,
                                          weak_factory_.GetWeakPtr(), request));
}

void IntentGenerator::LoadModelCallback(const QuickAnswersRequest& request,
                                        LoadModelResult result) {
  if (result != LoadModelResult::OK) {
    LOG(ERROR) << "Failed to load TextClassifier.";
    std::move(complete_callback_)
        .Run(request.selected_text, IntentType::kUnknown);
    return;
  }

  if (text_classifier_) {
    TextAnnotationRequestPtr text_annotation_request =
        machine_learning::mojom::TextAnnotationRequest::New();
    text_annotation_request->text = request.selected_text;
    text_annotation_request->default_locales =
        request.device_properties.language;

    text_classifier_->Annotate(
        std::move(text_annotation_request),
        base::BindOnce(&IntentGenerator::AnnotationCallback,
                       weak_factory_.GetWeakPtr(), request));
  }
}

void IntentGenerator::AnnotationCallback(
    const QuickAnswersRequest& request,
    std::vector<TextAnnotationPtr> annotations) {
  std::string entity_str;
  std::string type;

  if (ExtractEntity(request.selected_text, annotations, &entity_str, &type)) {
    auto intent_type_map = GetIntentTypeMap();
    auto it = intent_type_map.find(type);
    if (it != intent_type_map.end()) {
      std::move(complete_callback_)
          .Run(entity_str,
               RewriteIntent(request.selected_text, entity_str, it->second));
      return;
    }
  }
  // Fallback to language detection for generating translation intent.
  MaybeGenerateTranslationIntent(request);
}

void IntentGenerator::SetLanguageDetectorForTesting(
    std::unique_ptr<LanguageDetector> language_detector) {
  language_detector_ = std::move(language_detector);
}

void IntentGenerator::MaybeGenerateTranslationIntent(
    const QuickAnswersRequest& request) {
  DCHECK(complete_callback_);

  // Don't do language detection if no device language is provided or the length
  // of selected text is above the threshold. Returns unknown intent type.
  if (request.device_properties.language.empty() ||
      request.selected_text.length() > kTranslationTextLengthThreshold) {
    std::move(complete_callback_)
        .Run(request.selected_text, IntentType::kUnknown);
    return;
  }

  // TODO(b/150974962): Investigate improving language detection accuracy using
  // surrounding text or page content.
  auto detected_language =
      language_detector_->DetectLanguage(request.selected_text);

  auto intent_type = IntentType::kUnknown;
  if (!detected_language.empty() &&
      detected_language != request.device_properties.language) {
    intent_type = IntentType::kTranslation;
  }
  std::move(complete_callback_).Run(request.selected_text, intent_type);
}

}  // namespace quick_answers
}  // namespace chromeos