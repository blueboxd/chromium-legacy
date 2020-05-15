// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/settings_overridden_dialog_view.h"

#include "base/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/settings_overridden_dialog_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

SettingsOverriddenDialogView::SettingsOverriddenDialogView(
    std::unique_ptr<SettingsOverriddenDialogController> controller)
    : controller_(std::move(controller)) {
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(
                     IDS_EXTENSION_SETTINGS_OVERRIDDEN_DIALOG_CHANGE_IT_BACK));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(
                     IDS_EXTENSION_SETTINGS_OVERRIDDEN_DIALOG_IGNORE));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));

  using DialogResult = SettingsOverriddenDialogController::DialogResult;
  auto make_result_callback = [this](DialogResult result) {
    // NOTE: The following Bind's are safe because the callback is
    // owned by this object (indirectly, as a DialogDelegate).
    return base::BindOnce(
        &SettingsOverriddenDialogController::HandleDialogResult,
        base::Unretained(controller_.get()), result);
  };
  SetAcceptCallback(make_result_callback(DialogResult::kChangeSettingsBack));
  SetCancelCallback(make_result_callback(DialogResult::kKeepNewSettings));
  SetCloseCallback(make_result_callback(DialogResult::kDialogDismissed));

  SettingsOverriddenDialogController::ShowParams show_params =
      controller_->GetShowParams();

  SetTitle(show_params.dialog_title);

  auto message_label = std::make_unique<views::Label>(
      show_params.message, CONTEXT_BODY_TEXT_LARGE,
      views::style::STYLE_SECONDARY);
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(message_label));
}

SettingsOverriddenDialogView::~SettingsOverriddenDialogView() = default;

void SettingsOverriddenDialogView::Show(gfx::NativeWindow parent) {
  constrained_window::CreateBrowserModalDialogViews(this, parent)->Show();
  controller_->OnDialogShown();
}

ui::ModalType SettingsOverriddenDialogView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

gfx::Size SettingsOverriddenDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

namespace chrome {

void ShowExtensionSettingsOverriddenDialog(
    std::unique_ptr<SettingsOverriddenDialogController> controller,
    Browser* browser) {
  // Note: ownership is taken by the view hierarchy.
  auto* dialog_view = new SettingsOverriddenDialogView(std::move(controller));
  dialog_view->Show(browser->window()->GetNativeWindow());
}

}  // namespace chrome
