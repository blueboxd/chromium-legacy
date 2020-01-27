// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_suggestions_controller.h"

#include <utility>
#include <vector>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/cpp/assistant/proactive_suggestions.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/rand_util.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/features.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Conversation starters -------------------------------------------------------

constexpr int kMaxNumOfConversationStarters = 3;

}  // namespace

// AssistantSuggestionsController ----------------------------------------------

AssistantSuggestionsController::AssistantSuggestionsController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  if (chromeos::assistant::features::IsProactiveSuggestionsEnabled()) {
    proactive_suggestions_controller_ =
        std::make_unique<AssistantProactiveSuggestionsController>(
            assistant_controller_);
  }

  UpdateConversationStarters();
  assistant_controller_->AddObserver(this);
  AssistantState::Get()->AddObserver(this);
}

AssistantSuggestionsController::~AssistantSuggestionsController() {
  assistant_controller_->RemoveObserver(this);
  AssistantState::Get()->RemoveObserver(this);
}

void AssistantSuggestionsController::AddModelObserver(
    AssistantSuggestionsModelObserver* observer) {
  model_.AddObserver(observer);
}

void AssistantSuggestionsController::RemoveModelObserver(
    AssistantSuggestionsModelObserver* observer) {
  model_.RemoveObserver(observer);
}

void AssistantSuggestionsController::OnAssistantControllerConstructed() {
  assistant_controller_->ui_controller()->AddModelObserver(this);
}

void AssistantSuggestionsController::OnAssistantControllerDestroying() {
  assistant_controller_->ui_controller()->RemoveModelObserver(this);
}

void AssistantSuggestionsController::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  // When Assistant is finishing a session, we update our cache of conversation
  // starters so that they're fresh for the next launch.
  if (assistant::util::IsFinishingSession(new_visibility))
    UpdateConversationStarters();
}

void AssistantSuggestionsController::OnProactiveSuggestionsChanged(
    scoped_refptr<const ProactiveSuggestions> proactive_suggestions) {
  model_.SetProactiveSuggestions(std::move(proactive_suggestions));
}

void AssistantSuggestionsController::OnAssistantContextEnabled(bool enabled) {
  UpdateConversationStarters();
}

// TODO(dmblack): The conversation starter cache should receive its contents
// from the server. Hard-coding for the time being.
void AssistantSuggestionsController::UpdateConversationStarters() {
  using chromeos::assistant::mojom::AssistantSuggestion;
  using chromeos::assistant::mojom::AssistantSuggestionPtr;
  using chromeos::assistant::mojom::AssistantSuggestionType;

  std::vector<AssistantSuggestionPtr> conversation_starters;

  auto AddConversationStarter = [&conversation_starters](
                                    int message_id, GURL action_url = GURL()) {
    AssistantSuggestionPtr starter = AssistantSuggestion::New();
    starter->type = AssistantSuggestionType::kConversationStarter;
    starter->text = l10n_util::GetStringUTF8(message_id);
    starter->action_url = action_url;
    conversation_starters.push_back(std::move(starter));
  };

  // Always show the "What can you do?" conversation starter.
  AddConversationStarter(IDS_ASH_ASSISTANT_CHIP_WHAT_CAN_YOU_DO);

  // If enabled, always show the "What's on my screen?" conversation starter.
  if (AssistantState::Get()->context_enabled().value_or(false)) {
    AddConversationStarter(IDS_ASH_ASSISTANT_CHIP_WHATS_ON_MY_SCREEN,
                           assistant::util::CreateWhatsOnMyScreenDeepLink());
  }

  // The rest of the conversation starters will be shuffled...
  std::vector<int> shuffled_message_ids;

  shuffled_message_ids.push_back(IDS_ASH_ASSISTANT_CHIP_IM_BORED);
  shuffled_message_ids.push_back(IDS_ASH_ASSISTANT_CHIP_OPEN_FILES);
  shuffled_message_ids.push_back(IDS_ASH_ASSISTANT_CHIP_PLAY_MUSIC);
  shuffled_message_ids.push_back(IDS_ASH_ASSISTANT_CHIP_SEND_AN_EMAIL);
  shuffled_message_ids.push_back(IDS_ASH_ASSISTANT_CHIP_SET_A_REMINDER);
  shuffled_message_ids.push_back(IDS_ASH_ASSISTANT_CHIP_WHATS_ON_MY_CALENDAR);
  shuffled_message_ids.push_back(IDS_ASH_ASSISTANT_CHIP_WHATS_THE_WEATHER);

  base::RandomShuffle(shuffled_message_ids.begin(), shuffled_message_ids.end());

  // ...and added until we have no more than |kMaxNumOfConversationStarters|.
  for (int i = 0;
       conversation_starters.size() < kMaxNumOfConversationStarters &&
       i < static_cast<int>(shuffled_message_ids.size());
       ++i) {
    AddConversationStarter(shuffled_message_ids[i]);
  }

  model_.SetConversationStarters(std::move(conversation_starters));
}

}  // namespace ash
