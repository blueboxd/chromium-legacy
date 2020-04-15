// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/assistant_search_provider.h"

#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
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
using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;

// Constants.
constexpr char kIdPrefix[] = "googleassistant://";

// AssistantSearchResult -------------------------------------------------------

class AssistantSearchResult : public ChromeSearchResult {
 public:
  explicit AssistantSearchResult(
      const AssistantSuggestion* conversation_starter) {
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

  // TODO(b:153166883): Handle opening Assistant result.
  void Open(int event_flags) override { NOTIMPLEMENTED(); }
};

}  // namespace

// AssistantSearchProvider -----------------------------------------------------

AssistantSearchProvider::AssistantSearchProvider() {
  // Synchronize our initial state w/ that of the Assistant suggestions model.
  OnConversationStartersChanged(ash::AssistantSuggestionsController::Get()
                                    ->GetModel()
                                    ->GetConversationStarters());

  // Observe the Assistant suggestions model to receive updates.
  ash::AssistantSuggestionsController::Get()->AddModelObserver(this);
}

AssistantSearchProvider::~AssistantSearchProvider() {
  ash::AssistantSuggestionsController::Get()->RemoveModelObserver(this);
}

// TODO(b:153466226): Only create a result if confidence score threshold is met.
void AssistantSearchProvider::OnConversationStartersChanged(
    const std::vector<const AssistantSuggestion*>& conversation_starters) {
  SearchProvider::Results results;
  if (!conversation_starters.empty()) {
    const AssistantSuggestion* starter = conversation_starters.front();
    results.push_back(std::make_unique<AssistantSearchResult>(starter));
  }
  SwapResults(&results);
}

}  // namespace app_list
