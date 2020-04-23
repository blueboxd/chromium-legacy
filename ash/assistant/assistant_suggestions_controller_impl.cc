// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_suggestions_controller_impl.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/assistant/conversation_starter.h"
#include "ash/public/cpp/assistant/conversation_starters_client.h"
#include "ash/public/cpp/assistant/proactive_suggestions.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/unguessable_token.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

using chromeos::assistant::features::IsConversationStartersV2Enabled;
using chromeos::assistant::features::IsProactiveSuggestionsEnabled;
using chromeos::assistant::mojom::AssistantSuggestion;
using chromeos::assistant::mojom::AssistantSuggestionPtr;
using chromeos::assistant::mojom::AssistantSuggestionType;

// Conversation starters -------------------------------------------------------

constexpr int kMaxNumOfConversationStarters = 3;

bool IsAllowed(const ConversationStarter& conversation_starter) {
  using Permission = ConversationStarter::Permission;

  if (conversation_starter.RequiresPermission(Permission::kUnknown))
    return false;

  if (conversation_starter.RequiresPermission(Permission::kRelatedInfo) &&
      !AssistantState::Get()->context_enabled().value_or(false)) {
    return false;
  }

  return true;
}

AssistantSuggestionPtr ToAssistantSuggestionPtr(
    const ConversationStarter& conversation_starter) {
  AssistantSuggestionPtr ptr = AssistantSuggestion::New();
  ptr->id = base::UnguessableToken::Create();
  ptr->type = AssistantSuggestionType::kConversationStarter;
  ptr->text = conversation_starter.label();

  if (conversation_starter.action_url().has_value())
    ptr->action_url = conversation_starter.action_url().value();

  if (conversation_starter.icon_url().has_value())
    ptr->icon_url = conversation_starter.icon_url().value();

  return ptr;
}

}  // namespace

// AssistantSuggestionsControllerImpl ------------------------------------------

AssistantSuggestionsControllerImpl::AssistantSuggestionsControllerImpl(
    AssistantControllerImpl* assistant_controller)
    : assistant_controller_(assistant_controller) {
  if (IsProactiveSuggestionsEnabled()) {
    proactive_suggestions_controller_ =
        std::make_unique<AssistantProactiveSuggestionsController>(
            assistant_controller_);
  }

  // In conversation starters V2, we only update conversation starters when the
  // Assistant UI is becoming visible so as to maximize freshness.
  if (!IsConversationStartersV2Enabled())
    UpdateConversationStarters();

  assistant_controller_observer_.Add(AssistantController::Get());
}

AssistantSuggestionsControllerImpl::~AssistantSuggestionsControllerImpl() =
    default;

const AssistantSuggestionsModel* AssistantSuggestionsControllerImpl::GetModel()
    const {
  return &model_;
}

void AssistantSuggestionsControllerImpl::AddModelObserver(
    AssistantSuggestionsModelObserver* observer) {
  model_.AddObserver(observer);
}

void AssistantSuggestionsControllerImpl::RemoveModelObserver(
    AssistantSuggestionsModelObserver* observer) {
  model_.RemoveObserver(observer);
}

void AssistantSuggestionsControllerImpl::OnAssistantControllerConstructed() {
  AssistantUiController::Get()->AddModelObserver(this);
  AssistantState::Get()->AddObserver(this);
}

void AssistantSuggestionsControllerImpl::OnAssistantControllerDestroying() {
  AssistantState::Get()->RemoveObserver(this);
  AssistantUiController::Get()->RemoveModelObserver(this);
}

void AssistantSuggestionsControllerImpl::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  if (IsConversationStartersV2Enabled()) {
    // When Assistant is starting a session, we update our cache of conversation
    // starters so that they are as fresh as possible. Note that we may need to
    // modify this logic later if latency becomes a concern.
    if (assistant::util::IsStartingSession(new_visibility, old_visibility)) {
      UpdateConversationStarters();
      return;
    }
    // When Assistant is finishing a session, we clear our cache of conversation
    // starters so that, when the next session begins, we won't show stale
    // conversation starters while we fetch fresh ones.
    if (assistant::util::IsFinishingSession(new_visibility)) {
      conversation_starters_weak_factory_.InvalidateWeakPtrs();
      model_.SetConversationStarters({});
    }
    return;
  }

  DCHECK(!IsConversationStartersV2Enabled());

  // When Assistant is finishing a session, we update our cache of conversation
  // starters so that they're fresh for the next launch.
  if (assistant::util::IsFinishingSession(new_visibility))
    UpdateConversationStarters();
}

void AssistantSuggestionsControllerImpl::OnProactiveSuggestionsChanged(
    scoped_refptr<const ProactiveSuggestions> proactive_suggestions) {
  model_.SetProactiveSuggestions(std::move(proactive_suggestions));
}

void AssistantSuggestionsControllerImpl::OnAssistantContextEnabled(
    bool enabled) {
  // We currently assume that the context setting is not being modified while
  // Assistant UI is visible.
  DCHECK_NE(AssistantVisibility::kVisible,
            AssistantUiController::Get()->GetModel()->visibility());

  // In conversation starters V2, we only update conversation starters when
  // Assistant UI is becoming visible so as to maximize freshness.
  if (IsConversationStartersV2Enabled())
    return;

  UpdateConversationStarters();
}

void AssistantSuggestionsControllerImpl::UpdateConversationStarters() {
  // If conversation starters V2 is enabled, we'll fetch a fresh set of
  // conversation starters from the server.
  if (IsConversationStartersV2Enabled()) {
    FetchConversationStarters();
    return;
  }
  // Otherwise we'll use a locally provided set of proactive suggestions.
  ProvideConversationStarters();
}

void AssistantSuggestionsControllerImpl::FetchConversationStarters() {
  DCHECK(IsConversationStartersV2Enabled());

  // Invalidate any requests that are already in flight.
  conversation_starters_weak_factory_.InvalidateWeakPtrs();

  // Fetch a fresh set of conversation starters from the server (via the
  // dedicated ConversationStartersClient).
  ConversationStartersClient::Get()->FetchConversationStarters(base::BindOnce(
      [](const base::WeakPtr<AssistantSuggestionsControllerImpl>& self,
         std::vector<ConversationStarter>&& conversation_starters) {
        if (!self)
          return;

        // Remove any conversation starters which we determine to not be allowed
        // based on the required permissions that they specify. Note that this
        // no-ops if the collection is empty.
        base::EraseIf(conversation_starters,
                      [](const ConversationStarter& conversation_starter) {
                        return !IsAllowed(conversation_starter);
                      });

        // When the server doesn't respond with any conversation starters that
        // we can present, we'll fallback to the locally provided set.
        if (conversation_starters.empty()) {
          self->ProvideConversationStarters();
          return;
        }

        // The number of conversation starters should not exceed our maximum.
        while (conversation_starters.size() > kMaxNumOfConversationStarters)
          conversation_starters.pop_back();

        // We need to transform our conversation starters into the type that is
        // understood by the suggestions model...
        std::vector<AssistantSuggestionPtr> suggestions;
        std::transform(
            conversation_starters.begin(), conversation_starters.end(),
            std::back_inserter(suggestions), ToAssistantSuggestionPtr);

        // ...and we update our cache.
        self->model_.SetConversationStarters(std::move(suggestions));
      },
      conversation_starters_weak_factory_.GetWeakPtr()));
}

void AssistantSuggestionsControllerImpl::ProvideConversationStarters() {
  std::vector<AssistantSuggestionPtr> conversation_starters;

  // Adds a conversation starter for the given |message_id| and |action_url|.
  auto AddConversationStarter = [&conversation_starters](
                                    int message_id, GURL action_url = GURL()) {
    AssistantSuggestionPtr starter = AssistantSuggestion::New();
    starter->id = base::UnguessableToken::Create();
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
