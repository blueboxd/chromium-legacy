// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/assistant_search_provider.h"

#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

namespace app_list {

namespace {

// Aliases.
using AssistantAllowedState = ash::mojom::AssistantAllowedState;
using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;

// Constants.
constexpr char kIdPrefix[] = "googleassistant://";

// Helpers ---------------------------------------------------------------------

// Returns if the Assistant search provider is allowed to contribute results.
bool AreResultsAllowed() {
  ash::AssistantState* assistant_state = ash::AssistantState::Get();
  return assistant_state->allowed_state() == AssistantAllowedState::ALLOWED &&
         assistant_state->settings_enabled() == true;
}

// AssistantSearchResult -------------------------------------------------------

class AssistantSearchResult : public ChromeSearchResult {
 public:
  explicit AssistantSearchResult(
      const AssistantSuggestion* conversation_starter)
      : action_url_(conversation_starter->action_url) {
    set_id(kIdPrefix + conversation_starter->id.ToString());
    SetDisplayIndex(ash::SearchResultDisplayIndex::kFirstIndex);
    SetDisplayType(ash::SearchResultDisplayType::kChip);
    SetResultType(ash::AppListSearchResultType::kAssistantChip);
    SetTitle(base::UTF8ToUTF16(conversation_starter->text));
    SetChipIcon(gfx::CreateVectorIcon(
        ash::kAssistantIcon,
        ash::AppListConfig::instance().suggestion_chip_icon_dimension(),
        gfx::kPlaceholderColor));
  }

  AssistantSearchResult(const AssistantSearchResult&) = delete;
  AssistantSearchResult& operator=(const AssistantSearchResult&) = delete;
  ~AssistantSearchResult() override = default;

 private:
  // ChromeSearchResult:
  ash::SearchResultType GetSearchResultType() const override {
    return ash::SearchResultType::ASSISTANT;
  }

  // TODO(b:154152631): Prevent eager dismissal of launcher when opening.
  // TODO(b:154153233): Create and utilize new Assistant entry point.
  void Open(int event_flags) override {
    // Opening of |action_url_| is delegated to the Assistant controller as only
    // the Assistant controller knows how to handle Assistant deep links.
    ash::AssistantController::Get()->OpenUrl(action_url_);
  }

  const GURL action_url_;
};

}  // namespace

// AssistantSearchProvider -----------------------------------------------------

AssistantSearchProvider::AssistantSearchProvider() {
  UpdateResults();

  // Bind observers.
  state_observer_.Add(ash::AssistantState::Get());
  suggestions_observer_.Add(ash::AssistantSuggestionsController::Get());
}

AssistantSearchProvider::~AssistantSearchProvider() = default;

void AssistantSearchProvider::OnAssistantFeatureAllowedChanged(
    ash::mojom::AssistantAllowedState allowed_state) {
  UpdateResults();
}

void AssistantSearchProvider::OnAssistantSettingsEnabled(bool enabled) {
  UpdateResults();
}

void AssistantSearchProvider::OnConversationStartersChanged(
    const std::vector<const AssistantSuggestion*>& conversation_starters) {
  UpdateResults();
}

// TODO(b:153466226): Only create a result if confidence score threshold is met.
void AssistantSearchProvider::UpdateResults() {
  if (!AreResultsAllowed()) {
    ClearResults();
    return;
  }

  std::vector<const AssistantSuggestion*> conversation_starters =
      ash::AssistantSuggestionsController::Get()
          ->GetModel()
          ->GetConversationStarters();

  SearchProvider::Results results;
  if (!conversation_starters.empty()) {
    const AssistantSuggestion* starter = conversation_starters.front();
    results.push_back(std::make_unique<AssistantSearchResult>(starter));
  }
  SwapResults(&results);
}

}  // namespace app_list
