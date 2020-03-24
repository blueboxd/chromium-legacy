// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_UNDERSTANDING_INTENT_GENERATOR_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_UNDERSTANDING_INTENT_GENERATOR_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "chromeos/components/quick_answers/utils/language_detector.h"

namespace chromeos {
namespace quick_answers {

struct QuickAnswersRequest;
enum class IntentType;

// Generate intent from the |QuickAnswersRequest|.
class IntentGenerator {
 public:
  // Callback used when intent generation is complete.
  using IntentGeneratorCallback =
      base::OnceCallback<void(const std::string& text, IntentType type)>;

  explicit IntentGenerator(IntentGeneratorCallback complete_callback);

  IntentGenerator(const IntentGenerator&) = delete;
  IntentGenerator& operator=(const IntentGenerator&) = delete;

  ~IntentGenerator();

  // Generate intent from the |request|.
  void GenerateIntent(const QuickAnswersRequest& request);

  void SetLanguageDetectorForTesting(
      std::unique_ptr<LanguageDetector> language_detector);

 private:
  IntentGeneratorCallback complete_callback_;
  std::unique_ptr<LanguageDetector> language_detector_;
};

}  // namespace quick_answers
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_UNDERSTANDING_INTENT_GENERATOR_H_
