// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ui/suggestion_window_view.h"

#include <stddef.h>

#include <string>

#include "base/i18n/number_formatting.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/ui/assistive_delegate.h"
#include "chrome/browser/chromeos/input_method/ui/border_factory.h"
#include "chrome/browser/chromeos/input_method/ui/suggestion_details.h"
#include "chrome/browser/chromeos/input_method/ui/suggestion_view.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/wm/core/window_animations.h"

namespace ui {
namespace ime {

const int kSettingLinkFontSize = 13;
// TODO(crbug/1094843): Add localised string.
const char kSettingLinkLabel[] = "Why am I seeing this suggestion?";

class SettingLinkView : public views::View {
 public:
  explicit SettingLinkView(AssistiveDelegate* delegate) : delegate_(delegate) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    setting_link_ = AddChildView(
        std::make_unique<views::Link>(base::UTF8ToUTF16(kSettingLinkLabel)));
    setting_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    setting_link_->SetFontList(gfx::FontList({kFontStyle}, gfx::Font::ITALIC,
                                             kSettingLinkFontSize,
                                             gfx::Font::Weight::NORMAL));
    setting_link_->set_callback(base::BindRepeating(
        &SettingLinkView::LinkClicked, base::Unretained(this)));
  }

 private:
  AssistiveDelegate* delegate_;
  views::Link* setting_link_;

  void LinkClicked() {
    AssistiveWindowButton button;
    button.id = ButtonId::kSmartInputsSettingLink;
    delegate_->AssistiveWindowButtonClicked(button);
  }

  void Layout() override {
    setting_link_->SetBounds(kPadding, 0, width(), height());
  }

  gfx::Size CalculatePreferredSize() const override {
    gfx::Size size;
    size.Enlarge(setting_link_->GetPreferredSize().width() + 2 * kPadding, 0);
    return size;
  }

  DISALLOW_COPY_AND_ASSIGN(SettingLinkView);
};

SuggestionWindowView::SuggestionWindowView(gfx::NativeView parent,
                                           AssistiveDelegate* delegate)
    : delegate_(delegate) {
  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
  SetCanActivate(false);
  DCHECK(parent);
  set_parent_window(parent);
  set_margins(gfx::Insets());

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  candidate_area_ = AddChildView(std::make_unique<views::View>());
  candidate_area_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  setting_link_view_ =
      AddChildView(std::make_unique<SettingLinkView>(delegate));
  setting_link_view_->SetVisible(false);
}

SuggestionWindowView::~SuggestionWindowView() = default;

views::Widget* SuggestionWindowView::InitWidget() {
  views::Widget* widget = BubbleDialogDelegateView::CreateBubble(this);

  wm::SetWindowVisibilityAnimationTransition(widget->GetNativeView(),
                                             wm::ANIMATE_NONE);

  GetBubbleFrameView()->SetBubbleBorder(
      GetBorderForWindow(WindowBorderType::Suggestion));
  GetBubbleFrameView()->OnThemeChanged();
  return widget;
}

void SuggestionWindowView::Hide() {
  GetWidget()->Close();
}

void SuggestionWindowView::MakeVisible() {
  candidate_area_->SetVisible(true);
  SizeToContents();
}

void SuggestionWindowView::Show(const SuggestionDetails& details) {
  MaybeInitializeSuggestionViews(1);
  candidate_views_[0]->SetEnabled(true);
  candidate_views_[0]->SetView(details);
  if (details.show_setting_link) {
    candidate_views_[0]->SetMinWidth(
        setting_link_view_->GetPreferredSize().width());
  }
  setting_link_view_->SetVisible(details.show_setting_link);
  MakeVisible();
}

void SuggestionWindowView::ShowMultipleCandidates(
    const std::vector<base::string16>& candidates) {
  MaybeInitializeSuggestionViews(candidates.size());
  for (size_t i = 0; i < candidates.size(); i++) {
    SuggestionView* candidate_view = candidate_views_[i].get();
    candidate_view->SetViewWithIndex(base::FormatNumber(i + 1), candidates[i]);
    candidate_view->SetEnabled(true);
  }
  MakeVisible();
}

void SuggestionWindowView::MaybeInitializeSuggestionViews(
    size_t candidates_size) {
  UnhighlightCandidate(highlighted_index_);

  while (candidate_views_.size() > candidates_size) {
    candidate_views_.back()->RemoveButtonObserver(this);
    candidate_views_.pop_back();
  }

  while (candidate_views_.size() < candidates_size) {
    auto new_candidate = std::make_unique<SuggestionView>(this);
    candidate_area_->AddChildView(new_candidate.get());
    new_candidate->AddButtonObserver(this);
    candidate_views_.push_back(std::move(new_candidate));
  }
}

void SuggestionWindowView::SetButtonHighlighted(
    const AssistiveWindowButton& button,
    bool highlighted) {
  switch (button.id) {
    case ButtonId::kSuggestion:
      if (highlighted) {
        HighlightCandidate(button.index);
      } else {
        UnhighlightCandidate(button.index);
      }
      break;
    default:
      break;
  }
}

void SuggestionWindowView::HighlightCandidate(int index) {
  if (index == highlighted_index_ || index < 0 ||
      index >= static_cast<int>(candidate_views_.size())) {
    return;
  }

  UnhighlightCandidate(highlighted_index_);
  candidate_views_[index]->SetHighlighted(true);
  highlighted_index_ = index;
}

void SuggestionWindowView::UnhighlightCandidate(int index) {
  if (index != highlighted_index_ || index < 0 ||
      index >= static_cast<int>(candidate_views_.size())) {
    return;
  }

  candidate_views_[index]->SetHighlighted(false);
  highlighted_index_ = kInvalid;
}

void SuggestionWindowView::SetBounds(const gfx::Rect& cursor_bounds) {
  SetAnchorRect(cursor_bounds);
}

// TODO(crbug/1099116): Add test for ButtonPressed.
void SuggestionWindowView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  for (size_t i = 0; i < candidate_views_.size(); i++) {
    if (sender == candidate_views_[i].get()) {
      AssistiveWindowButton button;
      button.id = ui::ime::ButtonId::kSuggestion;
      button.index = i;
      delegate_->AssistiveWindowButtonClicked(button);
      return;
    }
  }
}

// TODO(crbug/1099062): Add tests for mouse hovered and pressed.
void SuggestionWindowView::OnStateChanged(
    views::Button* observed_button,
    views::Button::ButtonState old_state) {
  for (size_t i = 0; i < candidate_views_.size(); i++) {
    if (observed_button == candidate_views_[i].get()) {
      switch (observed_button->state()) {
        case views::Button::ButtonState::STATE_HOVERED:
        case views::Button::ButtonState::STATE_PRESSED:
          HighlightCandidate(i);
          break;
        default:
          UnhighlightCandidate(i);
      }
      return;
    }
  }
}

views::View* SuggestionWindowView::GetCandidateAreaForTesting() {
  return candidate_area_;
}

const char* SuggestionWindowView::GetClassName() const {
  return "SuggestionWindowView";
}

}  // namespace ime
}  // namespace ui
