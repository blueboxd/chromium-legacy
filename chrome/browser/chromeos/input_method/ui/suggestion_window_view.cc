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

namespace {
constexpr int kSuggestionWindowCornerRadius = 5;

class SuggestionWindowBorder : public views::BubbleBorder {
 public:
  SuggestionWindowBorder()
      : views::BubbleBorder(views::BubbleBorder::NONE,
                            views::BubbleBorder::SMALL_SHADOW,
                            SK_ColorTRANSPARENT),
        offset_(0) {
    SetCornerRadius(kSuggestionWindowCornerRadius);
    set_use_theme_background_color(true);
  }
  ~SuggestionWindowBorder() override {}

  void set_offset(int offset) { offset_ = offset; }

 private:
  int offset_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionWindowBorder);
};

}  // namespace

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
    delegate_->AssistiveWindowButtonClicked(ButtonId::kSmartInputsSettingLink,
                                            AssistiveWindowType::kNone);
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
                                           AssistiveDelegate* delegate) {
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
      std::make_unique<SuggestionWindowBorder>());
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

void SuggestionWindowView::Show(const base::string16& text,
                                const size_t confirmed_length,
                                const bool show_tab) {
  MaybeInitializeSuggestionViews(1);
  candidate_views_[0]->SetEnabled(true);
  candidate_views_[0]->SetView(text, confirmed_length, show_tab);
  candidate_views_[0]->SetMinWidth(
      setting_link_view_->GetPreferredSize().width());
  setting_link_view_->SetVisible(true);
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
  if (candidate_views_.size() > candidates_size)
    candidate_views_.resize(candidates_size);

  while (candidate_views_.size() < candidates_size) {
    auto new_candidate = std::make_unique<SuggestionView>();
    candidate_area_->AddChildView(new_candidate.get());
    candidate_views_.push_back(std::move(new_candidate));
  }
}

void SuggestionWindowView::HighlightCandidate(int index) {
  if (selected_index_ != -1)
    candidate_views_[selected_index_]->SetHighlighted(false);
  if (index < static_cast<int>(candidate_views_.size())) {
    candidate_views_[index]->SetHighlighted(true);
    selected_index_ = index;
  }
}

void SuggestionWindowView::SetBounds(const gfx::Rect& cursor_bounds) {
  SetAnchorRect(cursor_bounds);
}

const char* SuggestionWindowView::GetClassName() const {
  return "SuggestionWindowView";
}

}  // namespace ime
}  // namespace ui
