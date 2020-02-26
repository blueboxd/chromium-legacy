// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_MODEL_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_MODEL_H_

#include <string>
#include <vector>

#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"

namespace chromeos {
namespace quick_answers {

// The status of loading quick answers.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Note: Enums labels are at |QuickAnswersLoadStatus|.
enum class LoadStatus {
  kSuccess = 0,
  kNetworkError = 1,
  kNoResult = 2,
  kMaxValue = kNoResult,
};

// The type of the result. Valid values are map to the search result types.
// Please see go/1ns-doc for more detail.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Note: Enums labels are at |QuickAnswersResultType|.
enum class ResultType {
  kNoResult = 0,
  kKnowledgePanelEntityResult = 3982,
  kDefinitionResult = 5493,
  kTranslationResult = 6613,
  kUnitConversionResult = 13668,
};

// The predicted intent of the request.
enum class IntentType {
  kUnknown = 0,
  kUnit = 1,
  kDictionary = 2,
  kTranslation = 3
};

enum class QuickAnswerUiElementType {
  kUnknown = 0,
  kText = 1,
  kImage = 2,
};

class QuickAnswerUiElement {
 public:
  explicit QuickAnswerUiElement(QuickAnswerUiElementType type) : type_(type) {}
  QuickAnswerUiElement(const QuickAnswerUiElement&) = default;
  QuickAnswerUiElement& operator=(const QuickAnswerUiElement&) = default;
  QuickAnswerUiElement(QuickAnswerUiElement&&) = default;

  QuickAnswerUiElementType type() const { return type_; }

 private:
  QuickAnswerUiElementType type_ = QuickAnswerUiElementType::kUnknown;
};

// class to describe an answer text.
class QuickAnswerText : public QuickAnswerUiElement {
 public:
  QuickAnswerText(const std::string& text, SkColor color = SK_ColorBLACK)
      : QuickAnswerUiElement(QuickAnswerUiElementType::kText),
        text_(text),
        color_(color) {}

  const std::string text_;

  // Attributes for text style.
  SkColor color_ = SK_ColorBLACK;
};

class QuickAnswerImage : public QuickAnswerUiElement {
 public:
  explicit QuickAnswerImage(const gfx::Image& image)
      : QuickAnswerUiElement(QuickAnswerUiElementType::kImage), image_(image) {}

  gfx::Image image_;
};

// Structure to describe a quick answer.
struct QuickAnswer {
  QuickAnswer();
  ~QuickAnswer();

  //  TODO: Remove these after we deprecate simple UI version.
  ResultType result_type;
  std::string primary_answer;
  std::string secondary_answer;

  std::vector<std::unique_ptr<QuickAnswerUiElement>> title;
  std::vector<std::unique_ptr<QuickAnswerUiElement>> first_answer_row;
  std::vector<std::unique_ptr<QuickAnswerUiElement>> second_answer_row;
  std::unique_ptr<QuickAnswerImage> image;
};

// Structure to describe an quick answer request including selected content and
// context.
struct QuickAnswersRequest {
  // The selected Text.
  std::string selected_text;

  // TODO(llin): Add context and other targeted objects (e.g: images, links,
  // etc).
};

}  // namespace quick_answers
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_MODEL_H_
