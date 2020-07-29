// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/auth_dialog_debug_view.h"

#include <memory>
#include <utility>

#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {

enum class ButtonId {
  kMoreOptions,
  kCancel,
};

const char kTitle[] = "Verify it's you";
const char kFingerprintPrompt[] = "Touch the fingerprint sensor";
// If fingerprint option is available, password input field will be hidden
// until the user taps the MoreOptions button.
const char kMoreOptionsButtonText[] = "More options";
const char kCancelButtonText[] = "Cancel";

const int kContainerPreferredWidth = 512;
const int kTopVerticalSpacing = 24;
const int kVerticalSpacingBetweenTitleAndPrompt = 16;
const int kVerticalSpacingBetweenPromptAndButtons = 32;
const int kBottomVerticalSpacing = 20;
const int kButtonSpacing = 8;

const int kTitleFontSize = 14;
const int kPromptFontSize = 12;

}  // namespace

AuthDialogDebugView::AuthDialogDebugView() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  container_ = AddChildView(std::make_unique<NonAccessibleView>());
  container_->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));

  main_layout_ =
      container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  main_layout_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  main_layout_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  AddVerticalSpacing(kTopVerticalSpacing);
  AddTitleView();
  AddVerticalSpacing(kVerticalSpacingBetweenTitleAndPrompt);
  AddPromptView();
  AddVerticalSpacing(kVerticalSpacingBetweenPromptAndButtons);
  AddActionButtonsView();
  AddVerticalSpacing(kBottomVerticalSpacing);
}

AuthDialogDebugView::~AuthDialogDebugView() = default;

void AuthDialogDebugView::AddTitleView() {
  title_ = container_->AddChildView(std::make_unique<views::Label>());
  title_->SetEnabledColor(SK_ColorBLACK);
  title_->SetSubpixelRenderingEnabled(false);
  title_->SetAutoColorReadabilityEnabled(false);

  const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();

  title_->SetFontList(base_font_list.Derive(
      kTitleFontSize, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
  title_->SetText(base::UTF8ToUTF16(kTitle));
  title_->SetMaximumWidth(kContainerPreferredWidth);
  title_->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);
}

void AuthDialogDebugView::AddPromptView() {
  prompt_ = container_->AddChildView(std::make_unique<views::Label>());
  prompt_->SetEnabledColor(SK_ColorBLACK);
  prompt_->SetSubpixelRenderingEnabled(false);
  prompt_->SetAutoColorReadabilityEnabled(false);

  const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();

  prompt_->SetFontList(base_font_list.Derive(kPromptFontSize,
                                             gfx::Font::FontStyle::NORMAL,
                                             gfx::Font::Weight::NORMAL));
  // TODO(yichengli): Use a different prompt if the board has no fingerprint
  // sensor.
  prompt_->SetText(base::UTF8ToUTF16(kFingerprintPrompt));
  prompt_->SetMaximumWidth(kContainerPreferredWidth);
  prompt_->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);
}

void AuthDialogDebugView::AddVerticalSpacing(int height) {
  auto* spacing =
      container_->AddChildView(std::make_unique<NonAccessibleView>());
  spacing->SetPreferredSize(gfx::Size(kContainerPreferredWidth, height));
}

void AuthDialogDebugView::AddActionButtonsView() {
  action_view_container_ =
      container_->AddChildView(std::make_unique<NonAccessibleView>());
  auto* buttons_layout = action_view_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  buttons_layout->set_between_child_spacing(kButtonSpacing);

  more_options_button_ = AddButton(kMoreOptionsButtonText,
                                   static_cast<int>(ButtonId::kMoreOptions),
                                   action_view_container_);
  cancel_button_ =
      AddButton(kCancelButtonText, static_cast<int>(ButtonId::kCancel),
                action_view_container_);
}

void AuthDialogDebugView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  if (sender == cancel_button_) {
    // DestroyAuthenticationDialog deletes |this|.
    InSessionAuthDialogController::Get()->DestroyAuthenticationDialog();
  }

  // TODO(yichengli): Enable more options button when we have both fingerprint
  // view and password input view.
}

views::LabelButton* AuthDialogDebugView::AddButton(const std::string& text,
                                                   int id,
                                                   views::View* container) {
  // Creates a button with |text|.
  std::unique_ptr<views::LabelButton> button =
      views::MdTextButton::Create(this, base::ASCIIToUTF16(text));
  button->SetID(id);

  views::LabelButton* view = button.get();
  container->AddChildView(
      login_views_utils::WrapViewForPreferredSize(std::move(button)));
  return view;
}

}  // namespace ash
