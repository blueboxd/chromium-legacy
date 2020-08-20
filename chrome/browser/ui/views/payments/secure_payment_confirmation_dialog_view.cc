// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/secure_payment_confirmation_dialog_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/payments/content/secure_payment_confirmation_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"

namespace payments {
namespace {

// Height of the header icon.
constexpr int kHeaderIconHeight = 148;

// Height of the progress bar at the top of the dialog.
constexpr int kProgressBarHeight = 4;

}  // namespace

SecurePaymentConfirmationDialogView::SecurePaymentConfirmationDialogView(
    ObserverForTest* observer_for_test)
    : observer_for_test_(observer_for_test) {}
SecurePaymentConfirmationDialogView::~SecurePaymentConfirmationDialogView() =
    default;

void SecurePaymentConfirmationDialogView::ShowDialog(
    content::WebContents* web_contents,
    base::WeakPtr<SecurePaymentConfirmationModel> model,
    VerifyCallback verify_callback,
    CancelCallback cancel_callback) {
  DCHECK(model);
  model_ = model;

  InitChildViews();

  OnModelUpdated();

  verify_callback_ = std::move(verify_callback);
  cancel_callback_ = std::move(cancel_callback);

  SetAcceptCallback(
      base::BindOnce(&SecurePaymentConfirmationDialogView::OnDialogAccepted,
                     weak_ptr_factory_.GetWeakPtr()));
  SetCancelCallback(
      base::BindOnce(&SecurePaymentConfirmationDialogView::OnDialogCancelled,
                     weak_ptr_factory_.GetWeakPtr()));
  SetCloseCallback(
      base::BindOnce(&SecurePaymentConfirmationDialogView::OnDialogClosed,
                     weak_ptr_factory_.GetWeakPtr()));

  constrained_window::ShowWebModalDialogViews(this, web_contents);

  if (observer_for_test_)
    observer_for_test_->OnDialogOpened();
}

void SecurePaymentConfirmationDialogView::OnDialogAccepted() {
  std::move(verify_callback_).Run();

  if (observer_for_test_) {
    observer_for_test_->OnConfirmButtonPressed();
    observer_for_test_->OnDialogClosed();
  }
}

void SecurePaymentConfirmationDialogView::OnDialogCancelled() {
  std::move(cancel_callback_).Run();

  if (observer_for_test_) {
    observer_for_test_->OnCancelButtonPressed();
    observer_for_test_->OnDialogClosed();
  }
}

void SecurePaymentConfirmationDialogView::OnDialogClosed() {
  std::move(cancel_callback_).Run();

  if (observer_for_test_) {
    observer_for_test_->OnDialogClosed();
  }
}

void SecurePaymentConfirmationDialogView::OnModelUpdated() {
  // Changing the progress bar visibility does not invalidate layout as it is
  // absolutely positioned.
  if (progress_bar_)
    progress_bar_->SetVisible(model_->progress_bar_visible());

  SetButtonLabel(ui::DIALOG_BUTTON_OK, model_->verify_button_label());
  SetButtonEnabled(ui::DIALOG_BUTTON_OK, model_->verify_button_enabled());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, model_->cancel_button_label());
  SetButtonEnabled(ui::DIALOG_BUTTON_CANCEL, model_->cancel_button_enabled());
}

void SecurePaymentConfirmationDialogView::HideDialog() {
  if (GetWidget())
    GetWidget()->Close();
}

ui::ModalType SecurePaymentConfirmationDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

bool SecurePaymentConfirmationDialogView::ShouldShowCloseButton() const {
  return false;
}

base::WeakPtr<SecurePaymentConfirmationDialogView>
SecurePaymentConfirmationDialogView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

const gfx::VectorIcon&
SecurePaymentConfirmationDialogView::GetFingerprintIcon() {
  return GetNativeTheme()->ShouldUseDarkColors() ? kWebauthnFingerprintDarkIcon
                                                 : kWebauthnFingerprintIcon;
}

void SecurePaymentConfirmationDialogView::InitChildViews() {
  RemoveAllChildViews(true);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  std::unique_ptr<views::View> header_icon = CreateHeaderView();
  AddChildView(header_icon.release());

  InvalidateLayout();
}

std::unique_ptr<views::View>
SecurePaymentConfirmationDialogView::CreateHeaderView() {
  const int header_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  const gfx::Size header_size(header_width, kHeaderIconHeight);

  // The container view has no layout, so its preferred size is hardcoded to
  // match the size of the image, and the progress bar overlay is absolutely
  // positioned.
  auto header_view = std::make_unique<views::View>();
  header_view->SetPreferredSize(header_size);

  // Fingerprint header icon
  auto image_view = std::make_unique<NonAccessibleImageView>();
  gfx::IconDescription icon_description(GetFingerprintIcon());
  image_view->SetImage(gfx::CreateVectorIcon(icon_description));
  image_view->SetSize(header_size);
  image_view->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  image_view->SetID(DialogViewID::HEADER_ICON);
  header_view->AddChildView(image_view.release());

  // Progress bar
  auto progress_bar = std::make_unique<views::ProgressBar>(
      kProgressBarHeight, /*allow_round_corner=*/false);
  progress_bar->SetValue(-1);  // infinite animation.
  progress_bar->SetBackgroundColor(SK_ColorTRANSPARENT);
  progress_bar->SetPreferredSize(gfx::Size(header_width, kProgressBarHeight));
  progress_bar->SizeToPreferredSize();
  progress_bar->SetID(DialogViewID::PROGRESS_BAR);
  progress_bar->SetVisible(model_->progress_bar_visible());
  progress_bar_ = progress_bar.get();
  header_view->AddChildView(progress_bar.release());

  return header_view;
}

}  // namespace payments
