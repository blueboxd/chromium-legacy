// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_suggestions_model.h"

#include "ash/assistant/model/assistant_suggestions_model_observer.h"

namespace ash {

AssistantSuggestionsModel::AssistantSuggestionsModel() = default;

AssistantSuggestionsModel::~AssistantSuggestionsModel() = default;

void AssistantSuggestionsModel::AddObserver(
    AssistantSuggestionsModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantSuggestionsModel::RemoveObserver(
    AssistantSuggestionsModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantSuggestionsModel::SetConversationStarters(
    std::vector<AssistantSuggestionPtr> conversation_starters) {
  conversation_starters_.clear();
  conversation_starters_.swap(conversation_starters);

  NotifyConversationStartersChanged();
}

const chromeos::assistant::mojom::AssistantSuggestion*
AssistantSuggestionsModel::GetConversationStarterById(int id) const {
  // We consider the index of a conversation starter within our backing vector
  // to be its unique id.
  DCHECK_GE(id, 0);
  DCHECK_LT(id, static_cast<int>(conversation_starters_.size()));
  return conversation_starters_.at(id).get();
}

std::map<int, const chromeos::assistant::mojom::AssistantSuggestion*>
AssistantSuggestionsModel::GetConversationStarters() const {
  std::map<int, const AssistantSuggestion*> conversation_starters;

  // We use index within our backing vector to represent unique id.
  int id = 0;
  for (const AssistantSuggestionPtr& starter : conversation_starters_)
    conversation_starters[id++] = starter.get();

  return conversation_starters;
}

void AssistantSuggestionsModel::NotifyConversationStartersChanged() {
  const std::map<int, const AssistantSuggestion*> conversation_starters =
      GetConversationStarters();

  for (AssistantSuggestionsModelObserver& observer : observers_)
    observer.OnConversationStartersChanged(conversation_starters);
}

}  // namespace ash
