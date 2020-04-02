// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_save_unsynced_credentials_locally_view.h"

#include <numeric>
#include <utility>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/passwords/password_items_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/window/dialog_delegate.h"

PasswordSaveUnsyncedCredentialsLocallyView::
    PasswordSaveUnsyncedCredentialsLocallyView(
        content::WebContents* web_contents,
        views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/false),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  DialogDelegate::SetAcceptCallback(base::BindOnce(
      &SaveUnsyncedCredentialsLocallyBubbleController::OnSaveClicked,
      base::Unretained(&controller_)));
  // TODO(crbug.com/1062344): Add proper (translated) string.
  DialogDelegate::SetButtonLabel(ui::DIALOG_BUTTON_OK,
                                 base::ASCIIToUTF16("Save"));
  DialogDelegate::SetCancelCallback(base::BindOnce(
      &SaveUnsyncedCredentialsLocallyBubbleController::OnCancelClicked,
      base::Unretained(&controller_)));
  CreateLayout();
}

PasswordSaveUnsyncedCredentialsLocallyView::
    ~PasswordSaveUnsyncedCredentialsLocallyView() = default;

PasswordBubbleControllerBase*
PasswordSaveUnsyncedCredentialsLocallyView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase*
PasswordSaveUnsyncedCredentialsLocallyView::GetController() const {
  return &controller_;
}

void PasswordSaveUnsyncedCredentialsLocallyView::CreateLayout() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  for (const autofill::PasswordForm& row : controller_.unsynced_credentials()) {
    auto* row_view = AddChildView(std::make_unique<views::View>());
    auto* username_label = row_view->AddChildView(CreateUsernameLabel(row));
    auto* password_label = row_view->AddChildView(
        CreatePasswordLabel(row, IDS_PASSWORDS_VIA_FEDERATION, false));
    auto* row_layout =
        row_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    row_layout->SetFlexForView(username_label, 1);
    row_layout->SetFlexForView(password_label, 1);
  }
}

bool PasswordSaveUnsyncedCredentialsLocallyView::ShouldShowCloseButton() const {
  return true;
}

gfx::Size PasswordSaveUnsyncedCredentialsLocallyView::CalculatePreferredSize()
    const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}
