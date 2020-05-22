// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_suggestions_model.h"

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
  std::vector<const AssistantSuggestion*> conversation_starters;

  for (auto& conversation_starter : conversation_starters_)
    conversation_starters.push_back(conversation_starter.get());

  return conversation_starters;
}

void AssistantSuggestionsModel::NotifyConversationStartersChanged() {
  const std::vector<const AssistantSuggestion*> conversation_starters =
      GetConversationStarters();

  for (AssistantSuggestionsModelObserver& observer : observers_)
    observer.OnConversationStartersChanged(conversation_starters);
}

}  // namespace ash
