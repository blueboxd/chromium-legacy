// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_response.h"

#include <utility>

#include "ash/assistant/model/ui/assistant_ui_element.h"
#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"

namespace ash {

// AssistantResponse::Processor ------------------------------------------------

class AssistantResponse::Processor {
 public:
  Processor(AssistantResponse* response, ProcessingCallback callback)
      : response_(response), callback_(std::move(callback)) {}

  Processor(const Processor& copy) = delete;
  Processor& operator=(const Processor& assign) = delete;

  ~Processor() {
    if (callback_)
      std::move(callback_).Run(/*is_completed=*/false);
  }

  void Process() {
    // Responses should only be processed once.
    DCHECK_EQ(ProcessingState::kUnprocessed, response_->processing_state());
    response_->set_processing_state(ProcessingState::kProcessing);

    // Completion of |response_| processing is indicated by |processing_count_|
    // reaching zero. This value is decremented as each UI element is processed.
    processing_count_ = response_->GetUiElements().size();

    // Try finishing directly if there are no UI elements to be processed.
    if (processing_count_ == 0) {
      TryFinishing();
      return;
    }

    for (const auto& ui_element : response_->GetUiElements()) {
      // Start asynchronous processing of the UI element. Note that if the UI
      // element does not require any pre-rendering processing the callback may
      // be run synchronously. Also we must use WeakPtr here because |this| will
      // destroy before |ui_element| by design.
      ui_element->Process(
          base::BindOnce(&AssistantResponse::Processor::OnFinishedProcessing,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

 private:
  void OnFinishedProcessing() {
    // We handle success/failure cases the same because failures will be skipped
    // in view handling. We decrement our |processing_count_| and attempt to
    // finish response processing. This will no-op if elements are still
    // processing.
    --processing_count_;
    TryFinishing();
  }

  void TryFinishing() {
    // No-op if we are already finished or if elements are still processing.
    if (!callback_ || processing_count_ > 0)
      return;

    // Notify processing completion.
    response_->set_processing_state(ProcessingState::kProcessed);
    std::move(callback_).Run(/*is_completed=*/true);
  }

  // |response_| should outlive the Processor.
  AssistantResponse* const response_;
  ProcessingCallback callback_;

  int processing_count_ = 0;
  base::WeakPtrFactory<AssistantResponse::Processor> weak_ptr_factory_{this};
};

// AssistantResponse -----------------------------------------------------------

AssistantResponse::AssistantResponse() = default;

AssistantResponse::~AssistantResponse() {
  // Reset |processor_| explicitly in the destructor to guarantee the correct
  // lifecycle where |this| should outlive the |processor_|. This can also force
  // |processor_| to be destroyed before |ui_elements_| as we want regardless of
  // the declaration order.
  processor_.reset();
}

void AssistantResponse::AddUiElement(
    std::unique_ptr<AssistantUiElement> ui_element) {
  ui_elements_.push_back(std::move(ui_element));
}

const std::vector<std::unique_ptr<AssistantUiElement>>&
AssistantResponse::GetUiElements() const {
  return ui_elements_;
}

void AssistantResponse::AddSuggestions(
    std::vector<AssistantSuggestionPtr> suggestions) {
  for (AssistantSuggestionPtr& suggestion : suggestions)
    suggestions_.push_back(std::move(suggestion));
}

const chromeos::assistant::mojom::AssistantSuggestion*
AssistantResponse::GetSuggestionById(int id) const {
  // We consider the index of a suggestion within our backing vector to be its
  // unique identifier.
  DCHECK_GE(id, 0);
  DCHECK_LT(id, static_cast<int>(suggestions_.size()));
  return suggestions_.at(id).get();
}

std::map<int, const chromeos::assistant::mojom::AssistantSuggestion*>
AssistantResponse::GetSuggestions() const {
  std::map<int, const AssistantSuggestion*> suggestions;

  // We use index within our backing vector to represent the unique identifier
  // for a suggestion.
  int id = 0;
  for (const AssistantSuggestionPtr& suggestion : suggestions_)
    suggestions[id++] = suggestion.get();

  return suggestions;
}

void AssistantResponse::Process(ProcessingCallback callback) {
  processor_ = std::make_unique<Processor>(this, std::move(callback));
  processor_->Process();
}

}  // namespace ash
