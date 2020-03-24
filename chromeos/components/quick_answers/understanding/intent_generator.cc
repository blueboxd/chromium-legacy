// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/understanding/intent_generator.h"

#include <map>

#include "chromeos/components/quick_answers/quick_answers_model.h"

namespace chromeos {
namespace quick_answers {

IntentGenerator::IntentGenerator(IntentGeneratorCallback complete_callback)
    : complete_callback_(std::move(complete_callback)) {
  language_detector_ = std::make_unique<LanguageDetector>();
}

IntentGenerator::~IntentGenerator() {
  if (complete_callback_)
    std::move(complete_callback_).Run(std::string(), IntentType::kUnknown);
}

void IntentGenerator::GenerateIntent(const QuickAnswersRequest& request) {
  // TODO(llin): Generate intent based on text annotation result first and
  // fallback to language detection for generating translation intent. Text
  // annotation will be a async call.

  // Check if it is |kTranslation| intent.

  // Don't do language detection if no device language is provided. Returns
  // unknown intent type.
  if (request.device_properties.language.empty()) {
    std::move(complete_callback_)
        .Run(request.selected_text, IntentType::kUnknown);
    return;
  }

  // TODO(b/150974962): Investigate improving language detection accuracy using
  // surrounding text or page content.
  auto detected_language =
      language_detector_->DetectLanguage(request.selected_text);

  if (!detected_language.empty() &&
      detected_language != request.device_properties.language) {
    // Detected language is different from device language.
    std::move(complete_callback_)
        .Run(request.selected_text, IntentType::kTranslation);
  } else {
    std::move(complete_callback_)
        .Run(request.selected_text, IntentType::kUnknown);
  }
}

void IntentGenerator::SetLanguageDetectorForTesting(
    std::unique_ptr<LanguageDetector> language_detector) {
  language_detector_ = std::move(language_detector);
}

}  // namespace quick_answers
}  // namespace chromeos