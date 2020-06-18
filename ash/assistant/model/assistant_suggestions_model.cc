// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_suggestions_model.h"

#include <algorithm>

#include "ash/assistant/model/assistant_suggestions_model_observer.h"
#include "base/unguessable_token.h"

namespace ash {

AssistantSuggestionsModel::AssistantSuggestionsModel() = default;

AssistantSuggestionsModel::~AssistantSuggestionsModel() = default;

void AssistantSuggestionsModel::AddObserver(
    AssistantSuggestionsModelObserver* observer) const {
  observers_.AddObserver(observer);
}

void AssistantSuggestionsModel::RemoveObserver(
    AssistantSuggestionsModelObserver* observer) const {
  observers_.RemoveObserver(observer);
}

void AssistantSuggestionsModel::SetConversationStarters(
    std::vector<AssistantSuggestionPtr> conversation_starters) {
  conversation_starters_.clear();
  conversation_starters_.swap(conversation_starters);

  NotifyConversationStartersChanged();
}

const chromeos::assistant::mojom::AssistantSuggestion*
AssistantSuggestionsModel::GetConversationStarterById(
    const base::UnguessableToken& id) const {
  for (auto& conversation_starter : conversation_starters_) {
    if (conversation_starter->id == id)
      return conversation_starter.get();
  }
  return nullptr;
}

std::vector<const chromeos::assistant::mojom::AssistantSuggestion*>
AssistantSuggestionsModel::GetConversationStarters() const {
  // Transform |conversation_starters_| to a vector of *const* starters.
  std::vector<const AssistantSuggestion*> conversation_starters;
  std::transform(conversation_starters_.begin(), conversation_starters_.end(),
                 std::back_inserter(conversation_starters),
                 [](const auto& starter) { return starter.get(); });
  return conversation_starters;
}

void AssistantSuggestionsModel::SetOnboardingSuggestions(
    std::vector<AssistantSuggestionPtr> onboarding_suggestions) {
  onboarding_suggestions_.clear();
  onboarding_suggestions_.swap(onboarding_suggestions);

  NotifyOnboardingSuggestionsChanged();
}

std::vector<const chromeos::assistant::mojom::AssistantSuggestion*>
AssistantSuggestionsModel::GetOnboardingSuggestions() const {
  // Transform |onboarding_suggestions_| to a vector of *const* suggestions.
  std::vector<const AssistantSuggestion*> onboarding_suggestions;
  std::transform(onboarding_suggestions_.begin(), onboarding_suggestions_.end(),
                 std::back_inserter(onboarding_suggestions),
                 [](const auto& suggestion) { return suggestion.get(); });
  return onboarding_suggestions;
}

void AssistantSuggestionsModel::NotifyConversationStartersChanged() {
  const std::vector<const AssistantSuggestion*> conversation_starters =
      GetConversationStarters();

  for (AssistantSuggestionsModelObserver& observer : observers_)
    observer.OnConversationStartersChanged(conversation_starters);
}

void AssistantSuggestionsModel::NotifyOnboardingSuggestionsChanged() {
  const std::vector<const AssistantSuggestion*> onboarding_suggestions =
      GetOnboardingSuggestions();

  for (AssistantSuggestionsModelObserver& observer : observers_)
    observer.OnOnboardingSuggestionsChanged(onboarding_suggestions);
}

}  // namespace ash
