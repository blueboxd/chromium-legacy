// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_UI_ASSISTANT_CARD_ELEMENT_H_
#define ASH_ASSISTANT_MODEL_UI_ASSISTANT_CARD_ELEMENT_H_

#include <memory>
#include <string>

#include "ash/assistant/model/ui/assistant_ui_element.h"
#include "ash/public/cpp/assistant/assistant_web_view_2.h"
#include "base/component_export.h"
#include "base/macros.h"

namespace ash {

// AssistantCardElement --------------------------------------------------------

// An Assistant UI element that will be rendered as an HTML card.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantCardElement
    : public AssistantUiElement {
 public:
  using ProcessingCallback = base::OnceCallback<void(bool)>;

  explicit AssistantCardElement(const std::string& html,
                                const std::string& fallback);
  ~AssistantCardElement() override;

  const std::string& html() const { return html_; }
  const std::string& fallback() const { return fallback_; }

  const AssistantWebView2* contents_view() const {
    return contents_view_.get();
  }

  void set_contents_view(std::unique_ptr<AssistantWebView2> contents_view) {
    contents_view_ = std::move(contents_view);
  }

  // Invoke to begin processing the card element. Upon completion, the specified
  // |callback| will be run to indicate success or failure.
  void Process(ProcessingCallback callback);

 private:
  // Handles processing of an AssistantCardElement.
  class Processor : public AssistantWebView2::Observer {
   public:
    Processor(AssistantCardElement& card_element, ProcessingCallback callback);
    ~Processor() override;

    // Invoke to begin processing.
    void Process();

    // AssistantWebView2::Observer:
    void DidStopLoading() override;

   private:
    AssistantCardElement& card_element_;
    ProcessingCallback callback_;

    std::unique_ptr<AssistantWebView2> contents_view_;

    DISALLOW_COPY_AND_ASSIGN(Processor);
  };

  const std::string html_;
  const std::string fallback_;
  std::unique_ptr<AssistantWebView2> contents_view_;

  std::unique_ptr<Processor> processor_;

  DISALLOW_COPY_AND_ASSIGN(AssistantCardElement);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_UI_ASSISTANT_CARD_ELEMENT_H_
