// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/test/test_assistant_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/unguessable_token.h"
#include "chromeos/services/assistant/public/cpp/default_assistant_interaction_subscriber.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using chromeos::assistant::DefaultAssistantInteractionSubscriber;
using chromeos::assistant::mojom::AssistantInteractionMetadata;
using chromeos::assistant::mojom::AssistantInteractionMetadataPtr;
using chromeos::assistant::mojom::AssistantInteractionResolution;
using chromeos::assistant::mojom::AssistantInteractionSubscriber;
using chromeos::assistant::mojom::AssistantInteractionType;
using chromeos::assistant::mojom::AssistantSuggestion;
using chromeos::assistant::mojom::AssistantSuggestionPtr;

// Subscriber that will ensure the LibAssistant contract is enforced.
// More specifically, it will ensure that:
//    - A conversation is finished before starting a new one.
//    - No responses (text, card, ...) are sent before starting or after
//    finishing an interaction.
class SanityCheckSubscriber : public DefaultAssistantInteractionSubscriber {
 public:
  SanityCheckSubscriber() = default;
  ~SanityCheckSubscriber() override = default;

  // DefaultAssistantInteractionSubscriber implementation:
  void OnInteractionStarted(AssistantInteractionMetadataPtr metadata) override {
    if (current_state_ == ConversationState::kInProgress) {
      ADD_FAILURE()
          << "Cannot start a new Assistant interaction without finishing the "
             "previous interaction first.";
    }
    current_state_ = ConversationState::kInProgress;
  }

  void OnInteractionFinished(
      AssistantInteractionResolution resolution) override {
    // Note: We do not check |current_state_| here as this method can be called
    // even if no interaction is in progress.
    current_state_ = ConversationState::kFinished;
  }

  void OnHtmlResponse(const std::string& response,
                      const std::string& fallback) override {
    CheckResponse();
  }

  void OnSuggestionsResponse(
      std::vector<chromeos::assistant::mojom::AssistantSuggestionPtr> response)
      override {
    CheckResponse();
  }

  void OnTextResponse(const std::string& response) override { CheckResponse(); }

  void OnOpenUrlResponse(const ::GURL& url, bool in_background) override {
    CheckResponse();
  }

  void OnOpenAppResponse(chromeos::assistant::mojom::AndroidAppInfoPtr app_info,
                         OnOpenAppResponseCallback callback) override {
    CheckResponse();
  }

 private:
  void CheckResponse() {
    if (current_state_ == ConversationState::kNotStarted)
      ADD_FAILURE() << "Cannot send a response before starting an interaction.";
    if (current_state_ == ConversationState::kFinished) {
      ADD_FAILURE()
          << "Cannot send a response after finishing the interaction.";
    }
  }

  enum class ConversationState {
    kNotStarted,
    kInProgress,
    kFinished,
  };

  ConversationState current_state_ = ConversationState::kNotStarted;

  DISALLOW_COPY_AND_ASSIGN(SanityCheckSubscriber);
};

// Subscriber that tracks the current interaction.
class CurrentInteractionSubscriber
    : public DefaultAssistantInteractionSubscriber {
 public:
  CurrentInteractionSubscriber() = default;
  CurrentInteractionSubscriber(CurrentInteractionSubscriber&) = delete;
  CurrentInteractionSubscriber& operator=(CurrentInteractionSubscriber&) =
      delete;
  ~CurrentInteractionSubscriber() override = default;

  // DefaultAssistantInteractionSubscriber implementation:
  void OnInteractionStarted(AssistantInteractionMetadataPtr metadata) override {
    current_interaction_ = *metadata;
  }

  void OnInteractionFinished(
      AssistantInteractionResolution resolution) override {
    current_interaction_ = base::nullopt;
  }

  base::Optional<AssistantInteractionMetadata> current_interaction() {
    return current_interaction_;
  }

 private:
  base::Optional<AssistantInteractionMetadata> current_interaction_ =
      base::nullopt;
};

class InteractionResponse::Response {
 public:
  Response() = default;
  virtual ~Response() = default;

  virtual void SendTo(
      chromeos::assistant::mojom::AssistantInteractionSubscriber* receiver) = 0;
};

class TextResponse : public InteractionResponse::Response {
 public:
  explicit TextResponse(const std::string& text) : text_(text) {}
  ~TextResponse() override = default;

  void SendTo(chromeos::assistant::mojom::AssistantInteractionSubscriber*
                  receiver) override {
    receiver->OnTextResponse(text_);
  }

 private:
  std::string text_;

  DISALLOW_COPY_AND_ASSIGN(TextResponse);
};

class SuggestionsResponse : public InteractionResponse::Response {
 public:
  explicit SuggestionsResponse(const std::string& text) : text_(text) {}
  SuggestionsResponse(const SuggestionsResponse&) = delete;
  SuggestionsResponse& operator=(const SuggestionsResponse&) = delete;
  ~SuggestionsResponse() override = default;

  void SendTo(chromeos::assistant::mojom::AssistantInteractionSubscriber*
                  receiver) override {
    std::vector<AssistantSuggestionPtr> suggestions;
    suggestions.emplace_back(AssistantSuggestion::New());
    auto& suggestion = suggestions.back();
    suggestion->text = text_;
    suggestion->id = base::UnguessableToken::Create();
    receiver->OnSuggestionsResponse(std::move(suggestions));
  }

 private:
  std::string text_;
};

class ResolutionResponse : public InteractionResponse::Response {
 public:
  using Resolution = InteractionResponse::Resolution;

  explicit ResolutionResponse(Resolution resolution)
      : resolution_(resolution) {}
  ~ResolutionResponse() override = default;

  void SendTo(chromeos::assistant::mojom::AssistantInteractionSubscriber*
                  receiver) override {
    receiver->OnInteractionFinished(resolution_);
  }

 private:
  Resolution resolution_;

  DISALLOW_COPY_AND_ASSIGN(ResolutionResponse);
};

TestAssistantService::TestAssistantService()
    : sanity_check_subscriber_(std::make_unique<SanityCheckSubscriber>()),
      current_interaction_subscriber_(
          std::make_unique<CurrentInteractionSubscriber>()) {
  AddAssistantInteractionSubscriber(
      sanity_check_subscriber_->BindNewPipeAndPassRemote());
  AddAssistantInteractionSubscriber(
      current_interaction_subscriber_->BindNewPipeAndPassRemote());
}

TestAssistantService::~TestAssistantService() = default;

mojo::PendingRemote<chromeos::assistant::mojom::Assistant>
TestAssistantService::CreateRemoteAndBind() {
  return receiver_.BindNewPipeAndPassRemote();
}

void TestAssistantService::SetInteractionResponse(
    std::unique_ptr<InteractionResponse> response) {
  interaction_response_ = std::move(response);
}

base::Optional<AssistantInteractionMetadata>
TestAssistantService::current_interaction() {
  return current_interaction_subscriber_->current_interaction();
}

void TestAssistantService::StartEditReminderInteraction(
    const std::string& client_id) {}

void TestAssistantService::StartScreenContextInteraction(
    ax::mojom::AssistantStructurePtr assistant_structure,
    const std::vector<uint8_t>& assistant_screenshot) {}

void TestAssistantService::StartTextInteraction(
    const std::string& query,
    chromeos::assistant::mojom::AssistantQuerySource source,
    bool allow_tts) {
  StartInteraction(AssistantInteractionType::kText, source, query);
  if (interaction_response_)
    SendInteractionResponse();
}

void TestAssistantService::StartVoiceInteraction() {
  StartInteraction(AssistantInteractionType::kVoice);
  if (interaction_response_)
    SendInteractionResponse();
}

void TestAssistantService::StartWarmerWelcomeInteraction(
    int num_warmer_welcome_triggered,
    bool allow_tts) {}

void TestAssistantService::StopActiveInteraction(bool cancel_conversation) {
  if (!running_active_interaction_)
    return;

  running_active_interaction_ = false;
  for (auto& subscriber : interaction_subscribers_) {
    subscriber->OnInteractionFinished(
        AssistantInteractionResolution::kInterruption);
  }
}

void TestAssistantService::AddAssistantInteractionSubscriber(
    mojo::PendingRemote<AssistantInteractionSubscriber> subscriber) {
  interaction_subscribers_.Add(
      mojo::Remote<AssistantInteractionSubscriber>(std::move(subscriber)));
}

void TestAssistantService::RetrieveNotification(
    chromeos::assistant::mojom::AssistantNotificationPtr notification,
    int action_index) {}

void TestAssistantService::DismissNotification(
    chromeos::assistant::mojom::AssistantNotificationPtr notification) {}

void TestAssistantService::OnAccessibilityStatusChanged(
    bool spoken_feedback_enabled) {}

void TestAssistantService::SendAssistantFeedback(
    chromeos::assistant::mojom::AssistantFeedbackPtr feedback) {}

void TestAssistantService::NotifyEntryIntoAssistantUi(
    chromeos::assistant::mojom::AssistantEntryPoint entry_point) {
}

void TestAssistantService::AddTimeToTimer(const std::string& id,
                                          base::TimeDelta duration) {
}

void TestAssistantService::RemoveAlarmTimer(const std::string& id) {}

void TestAssistantService::StartInteraction(
    chromeos::assistant::mojom::AssistantInteractionType type,
    chromeos::assistant::mojom::AssistantQuerySource source,
    const std::string& query) {
  DCHECK(!running_active_interaction_);
  for (auto& subscriber : interaction_subscribers_) {
    subscriber->OnInteractionStarted(
        AssistantInteractionMetadata::New(type, source, query));
  }
  running_active_interaction_ = true;
}

void TestAssistantService::SendInteractionResponse() {
  DCHECK(interaction_response_);
  DCHECK(running_active_interaction_);
  for (auto& subscriber : interaction_subscribers_)
    interaction_response_->SendTo(subscriber.get());
  DCHECK(!current_interaction());
  interaction_response_.reset();
  running_active_interaction_ = false;
}

InteractionResponse::InteractionResponse() = default;
InteractionResponse::~InteractionResponse() = default;

InteractionResponse* InteractionResponse::AddTextResponse(
    const std::string& text) {
  AddResponse(std::make_unique<TextResponse>(text));
  return this;
}

InteractionResponse* InteractionResponse::AddSuggestionChip(
    const std::string& text) {
  AddResponse(std::make_unique<SuggestionsResponse>(text));
  return this;
}

InteractionResponse* InteractionResponse::AddResolution(Resolution resolution) {
  AddResponse(std::make_unique<ResolutionResponse>(resolution));
  return this;
}

void InteractionResponse::AddResponse(std::unique_ptr<Response> response) {
  responses_.push_back(std::move(response));
}

void InteractionResponse::SendTo(
    chromeos::assistant::mojom::AssistantInteractionSubscriber* receiver) {
  for (auto& response : responses_)
    response->SendTo(receiver);
}

}  // namespace ash
