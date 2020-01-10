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
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"

namespace safe_browsing {

namespace {

constexpr base::TimeDelta kInitialUIDelay =
    base::TimeDelta::FromMilliseconds(200);

constexpr base::TimeDelta kMinimumPendingDialogTime =
    base::TimeDelta::FromSeconds(2);

constexpr base::TimeDelta kSuccessDialogTimeout =
    base::TimeDelta::FromSeconds(1);

constexpr base::TimeDelta kResizeAnimationDuration =
    base::TimeDelta::FromMilliseconds(100);

// TODO(domfc): Update these colors.
constexpr SkColor kScanPendingColor = 0xff4285f4;
constexpr SkColor kScanSuccessColor = 0xff34a853;
constexpr SkColor kScanFailureColor = 0xffea4335;

constexpr int kImageSize = 100;

}  // namespace

DeepScanningDialogViews::DeepScanningDialogViews(
    std::unique_ptr<DeepScanningDialogDelegate> delegate,
    content::WebContents* web_contents)
    : delegate_(std::move(delegate)), web_contents_(web_contents) {
  // Show the pending dialog after a delay in case the response is fast enough.
  base::PostDelayedTask(FROM_HERE, {content::BrowserThread::UI},
                        base::BindOnce(&DeepScanningDialogViews::Show,
                                       weak_ptr_factory_.GetWeakPtr()),
                        kInitialUIDelay);
}

int DeepScanningDialogViews::GetDialogButtons() const {
  // TODO(domfc): Add "Learn more" button on scan failure.
  if (!scan_success_.has_value())
    return ui::DIALOG_BUTTON_CANCEL;
  else if (scan_success_.value())
    return ui::DIALOG_BUTTON_NONE;
  else
    return ui::DIALOG_BUTTON_CANCEL;
}

base::string16 DeepScanningDialogViews::GetWindowTitle() const {
  return base::string16();
}

bool DeepScanningDialogViews::Cancel() {
  delegate_->Cancel();
  return true;
}

bool DeepScanningDialogViews::ShouldShowCloseButton() const {
  return false;
}

views::View* DeepScanningDialogViews::GetContentsView() {
  return contents_view_.get();
}

views::Widget* DeepScanningDialogViews::GetWidget() {
  return contents_view_->GetWidget();
}

const views::Widget* DeepScanningDialogViews::GetWidget() const {
  return contents_view_->GetWidget();
}

void DeepScanningDialogViews::DeleteDelegate() {
  delete this;
}

ui::ModalType DeepScanningDialogViews::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

void DeepScanningDialogViews::ShowResult(bool success) {
  DCHECK(!scan_success_.has_value());
  scan_success_ = success;

  // Cleanup if the pending dialog wasn't shown and the verdict is safe.
  if (!shown_ && success) {
    delete this;
    return;
  }

  // Do nothing if the pending dialog wasn't shown, the delayed |Show| callback
  // will show the negative result later.
  if (!shown_ && !success)
    return;

  // Update the pending dialog only after it has been shown for a minimum amount
  // of time.
  base::TimeDelta time_shown = base::TimeTicks::Now() - first_shown_timestamp_;
  if (time_shown >= kMinimumPendingDialogTime) {
    UpdateDialog();
  } else {
    base::PostDelayedTask(FROM_HERE, {content::BrowserThread::UI},
                          base::BindOnce(&DeepScanningDialogViews::UpdateDialog,
                                         weak_ptr_factory_.GetWeakPtr()),
                          kMinimumPendingDialogTime - time_shown);
  }
}

void DeepScanningDialogViews::UpdateDialog() {
  DCHECK(shown_);
  DCHECK(scan_success_.has_value());

  // Update the buttons.
  SetupButtons();

  // Update the image. Currently only the color changes.
  image_->SetImage(gfx::CreateVectorIcon(vector_icons::kBusinessIcon,
                                         kImageSize, GetImageColor()));

  // Update the message.
  int old_text_lines = message_->GetRequiredLines();
  message_->SetText(GetDialogMessage());

  // Resize the dialog's height. This is needed since the button might be
  // removed (in the success case) and the text might take fewer or more lines.
  int new_text_lines = message_->GetRequiredLines();
  if (scan_success_.value() || (old_text_lines != new_text_lines))
    Resize(new_text_lines - old_text_lines);

  // Update the dialog.
  DialogDelegate::DialogModelChanged();
  widget_->ScheduleLayout();

  // Schedule the dialog to close itself in the success case.
  if (scan_success_.value()) {
    base::PostDelayedTask(FROM_HERE, {content::BrowserThread::UI},
                          base::BindOnce(&DialogDelegate::CancelDialog,
                                         weak_ptr_factory_.GetWeakPtr()),
                          kSuccessDialogTimeout);
  }
}

void DeepScanningDialogViews::Resize(int lines_delta) {
  DCHECK(scan_success_.has_value());

  gfx::Rect dialog_rect = widget_->GetContentsView()->GetContentsBounds();
  int new_height = dialog_rect.height();

  // Remove the button row's height if it's removed in the success case.
  if (scan_success_.value()) {
    DCHECK(contents_view_->parent());
    DCHECK_EQ(contents_view_->parent()->children().size(), 2ul);
    DCHECK_EQ(contents_view_->parent()->children()[0], contents_view_.get());

    views::View* button_row_view = contents_view_->parent()->children()[1];
    new_height -= button_row_view->GetContentsBounds().height();
  }

  // Apply the message lines delta.
  new_height += (lines_delta * message_->GetLineHeight());
  dialog_rect.set_height(new_height);

  // Setup the animation.
  bounds_animator_ =
      std::make_unique<views::BoundsAnimator>(widget_->GetRootView());
  bounds_animator_->SetAnimationDuration(kResizeAnimationDuration);

  DCHECK(widget_->GetRootView());
  DCHECK_EQ(widget_->GetRootView()->children().size(), 1ul);
  views::View* view_to_resize = widget_->GetRootView()->children()[0];

  // Start the animation.
  bounds_animator_->AnimateViewTo(view_to_resize, dialog_rect);

  // Change the widget's size.
  gfx::Size new_size = view_to_resize->size();
  new_size.set_height(new_height);
  widget_->SetSize(new_size);
}

void DeepScanningDialogViews::Show() {
  DCHECK(!shown_);
  shown_ = true;
  first_shown_timestamp_ = base::TimeTicks::Now();

  SetupButtons();

  contents_view_ = std::make_unique<views::View>();
  contents_view_->set_owned_by_client();

  // Create layout
  views::GridLayout* layout =
      contents_view_->SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(/*h_align=*/views::GridLayout::CENTER,
                     /*v_align=*/views::GridLayout::FILL,
                     /*resize_percent=*/1.0,
                     /*size_type=*/views::GridLayout::SizeType::USE_PREF,
                     /*fixed_width=*/0,
                     /*min_width=*/0);

  // Add the image.
  layout->StartRow(views::GridLayout::kFixedSize, 0);
  auto image = std::make_unique<views::ImageView>();
  image->SetImage(gfx::CreateVectorIcon(vector_icons::kBusinessIcon, kImageSize,
                                        GetImageColor()));
  image_ = layout->AddView(std::move(image));

  // Add the message.
  layout->StartRow(views::GridLayout::kFixedSize, 0);
  auto label = std::make_unique<views::Label>(GetDialogMessage());
  label->SetMultiLine(true);
  message_ = layout->AddView(std::move(label));

  // Add padding to distance the message from the button(s).
  layout->AddPaddingRow(views::GridLayout::kFixedSize, 10);

  widget_ = constrained_window::ShowWebModalDialogViews(this, web_contents_);
}

void DeepScanningDialogViews::SetupButtons() {
  if (!scan_success_.has_value() || !scan_success_.value()) {
    DialogDelegate::set_button_label(ui::DIALOG_BUTTON_CANCEL,
                                     GetCancelButtonText());
    DialogDelegate::set_default_button(ui::DIALOG_BUTTON_CANCEL);
  }

  // TODO(domfc): Add "Learn more" button setup for scan failures.
}

base::string16 DeepScanningDialogViews::GetDialogMessage() const {
  if (!scan_success_.has_value()) {
    return l10n_util::GetStringUTF16(
        IDS_DEEP_SCANNING_DIALOG_UPLOAD_PENDING_MESSAGE);
  } else if (scan_success_.value()) {
    return l10n_util::GetStringUTF16(IDS_DEEP_SCANNING_DIALOG_SUCCESS_MESSAGE);
  } else {
    return l10n_util::GetStringUTF16(
        IDS_DEEP_SCANNING_DIALOG_UPLOAD_FAILURE_MESSAGE);
  }
}

SkColor DeepScanningDialogViews::GetImageColor() const {
  if (!scan_success_.has_value())
    return kScanPendingColor;
  else if (scan_success_.value())
    return kScanSuccessColor;
  else
    return kScanFailureColor;
}

base::string16 DeepScanningDialogViews::GetCancelButtonText() const {
  if (!scan_success_.has_value())
    return l10n_util::GetStringUTF16(
        IDS_DEEP_SCANNING_DIALOG_CANCEL_UPLOAD_BUTTON);
  else if (!scan_success_.value())
    return l10n_util::GetStringUTF16(IDS_CLOSE);

  NOTREACHED();
  return base::string16();
}

DeepScanningDialogViews::~DeepScanningDialogViews() = default;

}  // namespace safe_browsing
