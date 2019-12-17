// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_views.h"

#include <memory>

#include "base/task/post_task.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/message_box_view.h"

namespace safe_browsing {

namespace {

constexpr base::TimeDelta kInitialUIDelay =
    base::TimeDelta::FromMilliseconds(200);

}  // namespace

DeepScanningDialogViews::DeepScanningDialogViews(
    std::unique_ptr<DeepScanningDialogDelegate> delegate,
    content::WebContents* web_contents)
    : delegate_(std::move(delegate)) {
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_CANCEL,
                                   l10n_util::GetStringUTF16(IDS_CANCEL));
  DialogDelegate::set_default_button(ui::DIALOG_BUTTON_CANCEL);

  views::MessageBoxView::InitParams init_params(
      l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_DIALOG_MESSAGE));
  init_params.inter_row_vertical_spacing =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL);
  message_box_view_ = new views::MessageBoxView(init_params);

  // Show the dialog after a delay in case the response is fast enough.
  base::PostDelayedTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&DeepScanningDialogViews::Show,
                     weak_ptr_factory_.GetWeakPtr(), web_contents),
      kInitialUIDelay);
}

int DeepScanningDialogViews::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

base::string16 DeepScanningDialogViews::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_DIALOG_TITLE);
}

bool DeepScanningDialogViews::Cancel() {
  delegate_->Cancel();
  return true;
}

bool DeepScanningDialogViews::ShouldShowCloseButton() const {
  return false;
}

views::View* DeepScanningDialogViews::GetContentsView() {
  return message_box_view_;
}

views::Widget* DeepScanningDialogViews::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* DeepScanningDialogViews::GetWidget() const {
  return message_box_view_->GetWidget();
}

void DeepScanningDialogViews::DeleteDelegate() {
  delete this;
}

ui::ModalType DeepScanningDialogViews::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

void DeepScanningDialogViews::CancelDialogIfShowing() {
  if (shown_) {
    DialogDelegate::CancelDialog();
  } else {
    // The UI not being shown means |message_box_view_| has to be freed here.
    delete message_box_view_;
    delete this;
  }
}

void DeepScanningDialogViews::Show(content::WebContents* web_contents) {
  constrained_window::ShowWebModalDialogViews(this, web_contents);
  shown_ = true;
}

DeepScanningDialogViews::~DeepScanningDialogViews() = default;

}  // namespace safe_browsing
