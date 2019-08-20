// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/suggestion_container_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kPreferredHeightDip = 48;

}  // namespace

// SuggestionContainerView -----------------------------------------------------

SuggestionContainerView::SuggestionContainerView(
    AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  InitLayout();

  // The AssistantViewDelegate should outlive SuggestionContainerView.
  delegate_->AddInteractionModelObserver(this);
  delegate_->AddSuggestionsModelObserver(this);
  delegate_->AddUiModelObserver(this);
}

SuggestionContainerView::~SuggestionContainerView() {
  delegate_->RemoveUiModelObserver(this);
  delegate_->RemoveSuggestionsModelObserver(this);
  delegate_->RemoveInteractionModelObserver(this);
}

const char* SuggestionContainerView::GetClassName() const {
  return "SuggestionContainerView";
}

gfx::Size SuggestionContainerView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int SuggestionContainerView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void SuggestionContainerView::OnContentsPreferredSizeChanged(
    views::View* content_view) {
  // Our contents should never be smaller than our container width because when
  // showing conversation starters we will be center aligned.
  const int width =
      std::max(content_view->GetPreferredSize().width(), this->width());
  content_view->SetSize(gfx::Size(width, kPreferredHeightDip));
}

void SuggestionContainerView::InitLayout() {
  layout_manager_ =
      content_view()->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPaddingDip), kSpacingDip));

  layout_manager_->set_cross_axis_alignment(
      app_list_features::IsEmbeddedAssistantUIEnabled()
          ? views::BoxLayout::CrossAxisAlignment::kCenter
          : views::BoxLayout::CrossAxisAlignment::kEnd);

  // We center align when showing conversation starters.
  layout_manager_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
}

void SuggestionContainerView::OnResponseChanged(
    const std::shared_ptr<AssistantResponse>& response) {
  has_received_response_ = true;

  OnSuggestionsCleared();

  // When no longer showing conversation starters, we start align our content.
  layout_manager_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);

  OnSuggestionsChanged(response->GetSuggestions());
}

void SuggestionContainerView::OnResponseCleared() {
  // Note that we don't reset |has_received_response_| here because that refers
  // to whether we've received a response during the current Assistant session,
  // not whether we are currently displaying a response.
  OnSuggestionsCleared();
}

void SuggestionContainerView::OnConversationStartersChanged(
    const std::map<int, const AssistantSuggestion*>& conversation_starters) {
  OnSuggestionsCleared();
  OnSuggestionsChanged(conversation_starters);
}

void SuggestionContainerView::OnSuggestionsChanged(
    const std::map<int, const AssistantSuggestion*>& suggestions) {
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;
  for (const std::pair<int, const AssistantSuggestion*>& suggestion :
       suggestions) {
    // We will use the same identifier by which the Assistant interaction model
    // uniquely identifies a suggestion to uniquely identify its corresponding
    // suggestion chip view.
    const int id = suggestion.first;

    SuggestionChipView::Params params;
    params.text = base::UTF8ToUTF16(suggestion.second->text);

    if (!suggestion.second->icon_url.is_empty()) {
      // Initiate a request to download the image for the suggestion chip icon.
      // Note that the request is identified by the suggestion id.
      delegate_->DownloadImage(
          suggestion.second->icon_url,
          base::BindOnce(
              &SuggestionContainerView::OnSuggestionChipIconDownloaded,
              download_request_weak_factory_.GetWeakPtr(), id));

      // To reserve layout space until the actual icon can be downloaded, we
      // supply an empty placeholder image to the suggestion chip view.
      params.icon = gfx::ImageSkia();
    }

    SuggestionChipView* suggestion_chip_view =
        new SuggestionChipView(params, /*listener=*/this);
    suggestion_chip_view->SetAccessibleName(params.text);

    // Given a suggestion chip view, we need to be able to look up the id of
    // the underlying suggestion. This is used for handling press events.
    suggestion_chip_view->SetID(id);

    // Given an id, we also want to be able to look up the corresponding
    // suggestion chip view. This is used for handling icon download events.
    suggestion_chip_views_[id] = suggestion_chip_view;

    content_view()->AddChildView(suggestion_chip_view);
  }
}

void SuggestionContainerView::OnSuggestionsCleared() {
  // Abort any download requests in progress.
  download_request_weak_factory_.InvalidateWeakPtrs();

  // When modifying the view hierarchy, make sure we keep our view cache synced.
  content_view()->RemoveAllChildViews(/*delete_children=*/true);
  suggestion_chip_views_.clear();
}

void SuggestionContainerView::OnSuggestionChipIconDownloaded(
    int id,
    const gfx::ImageSkia& icon) {
  if (!icon.isNull())
    suggestion_chip_views_[id]->SetIcon(icon);
}

void SuggestionContainerView::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  const AssistantSuggestion* suggestion = nullptr;

  // If we haven't yet received a query response, the suggestion chip that was
  // pressed was a conversation starter.
  if (!has_received_response_) {
    suggestion = delegate_->GetSuggestionsModel()->GetConversationStarterById(
        sender->GetID());
  } else {
    // Otherwise, the suggestion chip belonged to the interaction response.
    suggestion =
        delegate_->GetInteractionModel()->response()->GetSuggestionById(
            sender->GetID());
  }

  delegate_->OnSuggestionChipPressed(suggestion);
}

void SuggestionContainerView::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  if (assistant::util::IsStartingSession(new_visibility, old_visibility) &&
      entry_point.value() != AssistantEntryPoint::kLauncherSearchResult) {
    // Show conversation starters at the start of a new Assistant session except
    // when the user already started a query in Launcher quick search box (QSB).
    OnConversationStartersChanged(
        delegate_->GetSuggestionsModel()->GetConversationStarters());
    return;
  }

  if (!assistant::util::IsFinishingSession(new_visibility))
    return;

  // When Assistant is finishing a session, we need to reset view state.
  has_received_response_ = false;

  // When we start a new session we will be showing conversation starters so
  // we need to center align our content.
  layout_manager_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
}

}  // namespace ash
