// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_UI_ASSISTANT_CARD_ELEMENT_H_
#define ASH_ASSISTANT_MODEL_UI_ASSISTANT_CARD_ELEMENT_H_

#include <memory>
#include <string>

#include "ash/assistant/model/ui/assistant_ui_element.h"
#include "ash/public/cpp/assistant/assistant_web_view.h"
#include "base/component_export.h"
#include "base/macros.h"

namespace ash {

// An Assistant UI element that will be rendered as an HTML card.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantCardElement
    : public AssistantUiElement {
 public:
  explicit AssistantCardElement(const std::string& html,
                                const std::string& fallback);
  ~AssistantCardElement() override;

  // AssistantUiElement:
  void Process(ProcessingCallback callback) override;

  const std::string& html() const { return html_; }
  const std::string& fallback() const { return fallback_; }
  const AssistantWebView* contents_view() const { return contents_view_.get(); }

  void set_contents_view(std::unique_ptr<AssistantWebView> contents_view) {
    contents_view_ = std::move(contents_view);
  }

 private:
  class Processor;

  const std::string html_;
  const std::string fallback_;
  std::unique_ptr<AssistantWebView> contents_view_;

  std::unique_ptr<Processor> processor_;

  DISALLOW_COPY_AND_ASSIGN(AssistantCardElement);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_UI_ASSISTANT_CARD_ELEMENT_H_
