// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/move_to_account_store_bubble_view.h"
#include <algorithm>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/bubble_controllers/move_to_account_store_bubble_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {

std::unique_ptr<views::View> CreateHeaderImage(int image_id) {
  auto image_view = std::make_unique<NonAccessibleImageView>();
  image_view->SetImage(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(image_id));
  gfx::Size preferred_size = image_view->GetPreferredSize();
  if (preferred_size.width()) {
    preferred_size = gfx::ScaleToRoundedSize(
        preferred_size,
        static_cast<float>(ChromeLayoutProvider::Get()->GetDistanceMetric(
            DISTANCE_BUBBLE_PREFERRED_WIDTH)) /
            preferred_size.width());
    image_view->SetImageSize(preferred_size);
  }
  return image_view;
}

std::unique_ptr<views::Label> CreateDescription() {
  auto description = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MOVE_HINT),
      ChromeTextContext::CONTEXT_BODY_TEXT_LARGE, views::style::STYLE_HINT);
  description->SetMultiLine(true);
  description->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return description;
}

}  // namespace

MoveToAccountStoreBubbleView::MoveToAccountStoreBubbleView(
    content::WebContents* web_contents,
    views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*auto_dismissable=*/false),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kUnbounded,
                                   /*adjust_height_for_width=*/true))
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets(
              /*vertical=*/ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_CONTROL_LIST_VERTICAL),
              /*horizontal=*/0));

  AddChildView(CreateDescription());
  // TODO(crbug.com/1060128): Add images indicating "site"->"account" move.

  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MOVE_BUTTON));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(IDS_DECLINE_RECOVERY));
  SetAcceptCallback(
      base::BindOnce(&MoveToAccountStoreBubbleController::AcceptMove,
                     base::Unretained(&controller_)));
}

MoveToAccountStoreBubbleView::~MoveToAccountStoreBubbleView() = default;

void MoveToAccountStoreBubbleView::AddedToWidget() {
  static_cast<views::Label*>(GetBubbleFrameView()->title())
      ->SetAllowCharacterBreak(true);
}

void MoveToAccountStoreBubbleView::OnThemeChanged() {
  PasswordBubbleViewBase::OnThemeChanged();
  GetBubbleFrameView()->SetHeaderView(CreateHeaderImage(
      color_utils::IsDark(GetBubbleFrameView()->GetBackgroundColor())
          ? IDR_SAVE_PASSWORD_DARK
          : IDR_SAVE_PASSWORD));
}

gfx::Size MoveToAccountStoreBubbleView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

bool MoveToAccountStoreBubbleView::ShouldShowCloseButton() const {
  return true;
}

MoveToAccountStoreBubbleController*
MoveToAccountStoreBubbleView::GetController() {
  return &controller_;
}

const MoveToAccountStoreBubbleController*
MoveToAccountStoreBubbleView::GetController() const {
  return &controller_;
}
