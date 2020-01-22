// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_views.h"

#include <memory>

#include "base/task/post_task.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

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

constexpr SkColor kScanPendingColor = gfx::kGoogleBlue500;
constexpr SkColor kScanSuccessColor = gfx::kGoogleGreen500;
constexpr SkColor kScanFailureColor = gfx::kGoogleRed500;

constexpr SkColor kScanPendingSideImageColor = gfx::kGoogleBlue400;
constexpr SkColor kScanDoneSideImageColor = SkColorSetRGB(0xFF, 0xFF, 0xFF);

constexpr int kSideImageSize = 24;
constexpr int kTopImageSize = 100;

constexpr gfx::Insets kSideImageInsets = gfx::Insets(8, 8, 8, 8);
constexpr gfx::Insets kMessageAndIconRowInsets = gfx::Insets(0, 32, 0, 48);
constexpr int kSideIconBetweenChildSpacing = 16;

constexpr int kTextLineHeight = 20;

// A simple background class to show a colored circle behind the side icon once
// the scanning is done.
class CircleBackground : public views::Background {
 public:
  explicit CircleBackground(SkColor color) { SetNativeControlColor(color); }
  ~CircleBackground() override = default;

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    int radius = view->bounds().width() / 2;
    gfx::PointF center(radius, radius);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(get_color());
    canvas->DrawCircle(center, radius, flags);
  }
};

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

DeepScanningDialogViews::~DeepScanningDialogViews() = default;

const views::Widget* DeepScanningDialogViews::GetWidgetImpl() const {
  return contents_view_->GetWidget();
}

void DeepScanningDialogViews::UpdateDialog() {
  DCHECK(shown_);
  DCHECK(scan_success_.has_value());

  // Update the buttons.
  SetupButtons();

  // Update the top image. Currently only the color changes.
  image_->SetImage(gfx::CreateVectorIcon(vector_icons::kBusinessIcon,
                                         kTopImageSize, GetImageColor()));

  // Update the side icon by changing it's image color, adding a background and
  // removing the spinner.
  side_icon_image_->SetImage(gfx::CreateVectorIcon(
      vector_icons::kBusinessIcon, kSideImageSize, kScanDoneSideImageColor));
  side_icon_image_->SetBackground(
      std::make_unique<CircleBackground>(GetSideImageBackgroundColor()));
  side_icon_spinner_->parent()->RemoveChildView(side_icon_spinner_);
  delete side_icon_spinner_;

  // Update the message. Change the text color only if the scan was negative.
  if (!scan_success_.value())
    message_->SetEnabledColor(kScanFailureColor);
  message_->SetText(GetDialogMessage());

  // Resize the dialog's height. This is needed since the button might be
  // removed (in the success case) and the text might take fewer or more lines.
  int text_height = message_->GetRequiredLines() * message_->GetLineHeight();
  int row_height = message_->parent()->height();
  int height_to_add = std::max(text_height - row_height, 0);
  if (scan_success_.value() || (height_to_add > 0))
    Resize(height_to_add);

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

void DeepScanningDialogViews::Resize(int height_to_add) {
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
  new_height += height_to_add;
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

void DeepScanningDialogViews::SetupButtons() {
  if (!scan_success_.has_value() || !scan_success_.value()) {
    DialogDelegate::set_button_label(ui::DIALOG_BUTTON_CANCEL,
                                     GetCancelButtonText());
    DialogDelegate::set_default_button(ui::DIALOG_BUTTON_NONE);
  }

  // TODO(domfc): Add "Learn more" button setup for scan failures.
}

base::string16 DeepScanningDialogViews::GetDialogMessage() const {
  if (!scan_success_.has_value()) {
    return l10n_util::GetStringUTF16(
        IDS_DEEP_SCANNING_DIALOG_UPLOAD_PENDING_MESSAGE);
  }
  return l10n_util::GetStringUTF16(
      scan_success_.value() ? IDS_DEEP_SCANNING_DIALOG_SUCCESS_MESSAGE
                            : IDS_DEEP_SCANNING_DIALOG_UPLOAD_FAILURE_MESSAGE);
}

SkColor DeepScanningDialogViews::GetImageColor() const {
  if (!scan_success_.has_value())
    return kScanPendingColor;
  return scan_success_.value() ? kScanSuccessColor : kScanFailureColor;
}

base::string16 DeepScanningDialogViews::GetCancelButtonText() const {
  if (!scan_success_.has_value()) {
    return l10n_util::GetStringUTF16(
        IDS_DEEP_SCANNING_DIALOG_CANCEL_UPLOAD_BUTTON);
  }
  DCHECK(!scan_success_.value());
  return l10n_util::GetStringUTF16(IDS_CLOSE);
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
  columns->AddColumn(/*h_align=*/views::GridLayout::FILL,
                     /*v_align=*/views::GridLayout::FILL,
                     /*resize_percent=*/1.0,
                     /*size_type=*/views::GridLayout::SizeType::USE_PREF,
                     /*fixed_width=*/0,
                     /*min_width=*/0);

  // Add the top image.
  layout->StartRow(views::GridLayout::kFixedSize, 0);
  auto image = std::make_unique<views::ImageView>();
  image->SetImage(gfx::CreateVectorIcon(vector_icons::kBusinessIcon,
                                        kTopImageSize, GetImageColor()));
  image_ = layout->AddView(std::move(image));

  // Add padding to distance the top image from the icon and message.
  layout->AddPaddingRow(views::GridLayout::kFixedSize, 16);

  // Add the side icon and message row.
  layout->StartRow(views::GridLayout::kFixedSize, 0);
  auto icon_and_message_row = std::make_unique<views::View>();
  auto* row_layout =
      icon_and_message_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kMessageAndIconRowInsets,
          kSideIconBetweenChildSpacing));
  row_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  row_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Add the side icon.
  icon_and_message_row->AddChildView(CreateSideIcon());

  // Add the message.
  auto label = std::make_unique<views::Label>(GetDialogMessage());
  label->SetMultiLine(true);
  label->SetLineHeight(kTextLineHeight);
  label->SetVerticalAlignment(gfx::ALIGN_MIDDLE);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_ = icon_and_message_row->AddChildView(std::move(label));

  layout->AddView(std::move(icon_and_message_row));

  // Add padding to distance the message from the button(s).
  layout->AddPaddingRow(views::GridLayout::kFixedSize, 10);

  widget_ = constrained_window::ShowWebModalDialogViews(this, web_contents_);
}

std::unique_ptr<views::View> DeepScanningDialogViews::CreateSideIcon() {
  // The side icon is created either:
  // - When the pending dialog is shown
  // - When the response was fast enough that the failure dialog is shown first
  DCHECK(!scan_success_.has_value() || !scan_success_.value());

  // The icon left of the text has the appearance of a blue "Enterprise" logo
  // with a spinner when the scan is pending.
  auto icon = std::make_unique<views::View>();
  icon->SetLayoutManager(std::make_unique<views::FillLayout>());

  auto side_image = std::make_unique<views::ImageView>();
  side_image->SetImage(gfx::CreateVectorIcon(
      vector_icons::kBusinessIcon, kSideImageSize,
      scan_success_.has_value() ? kScanDoneSideImageColor
                                : kScanPendingSideImageColor));
  side_image->SetBorder(views::CreateEmptyBorder(kSideImageInsets));
  side_icon_image_ = icon->AddChildView(std::move(side_image));

  // Add a spinner if the scan result is pending, otherwise add a background.
  if (!scan_success_.has_value()) {
    auto spinner = std::make_unique<views::Throbber>();
    spinner->Start();
    side_icon_spinner_ = icon->AddChildView(std::move(spinner));
  } else {
    side_icon_image_->SetBackground(
        std::make_unique<CircleBackground>(GetSideImageBackgroundColor()));
  }

  return icon;
}

SkColor DeepScanningDialogViews::GetSideImageBackgroundColor() const {
  DCHECK(scan_success_.has_value());
  if (scan_success_.value())
    return kScanSuccessColor;
  else
    return kScanFailureColor;
}

}  // namespace safe_browsing
