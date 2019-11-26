// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_UI_ASSISTANT_CARD_ELEMENT_H_
#define ASH_ASSISTANT_MODEL_UI_ASSISTANT_CARD_ELEMENT_H_

#include <memory>
#include <string>

#include "ash/assistant/model/ui/assistant_ui_element.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "services/content/public/cpp/navigable_contents.h"

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

  const content::NavigableContents* contents() const { return contents_.get(); }
  content::NavigableContents* contents() { return contents_.get(); }

  void set_contents(std::unique_ptr<content::NavigableContents> contents) {
    contents_ = std::move(contents);
  }

  // Invoke to begin processing the card element. Upon completion, the specified
  // |callback| will be run to indicate success or failure.
  void Process(content::mojom::NavigableContentsFactory* contents_factory,
               ProcessingCallback callback);

 private:
  // Handles processing of an AssistantCardElement.
  class Processor : public content::NavigableContentsObserver {
   public:
    Processor(AssistantCardElement& card_element,
              content::mojom::NavigableContentsFactory* contents_factory,
              ProcessingCallback callback);
    ~Processor() override;

    // Invoke to begin processing.
    void Process();

    // content::NavigableContentsObserver:
    void DidStopLoading() override;

   private:
    AssistantCardElement& card_element_;
    content::mojom::NavigableContentsFactory* const contents_factory_;
    ProcessingCallback callback_;

    std::unique_ptr<content::NavigableContents> contents_;

    DISALLOW_COPY_AND_ASSIGN(Processor);
  };

  const std::string html_;
  const std::string fallback_;
  std::unique_ptr<content::NavigableContents> contents_;

  std::unique_ptr<Processor> processor_;

  DISALLOW_COPY_AND_ASSIGN(AssistantCardElement);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_UI_ASSISTANT_CARD_ELEMENT_H_
