// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/in_product_help/global_media_controls_in_product_help.h"
#include "chrome/browser/ui/in_product_help/global_media_controls_in_product_help_factory.h"
#include "chrome/browser/ui/views/feature_promos/global_media_controls_promo_controller.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button_controller.h"

MediaToolbarButtonView::MediaToolbarButtonView(
    const base::UnguessableToken& source_id,
    service_manager::Connector* connector,
    const Browser* browser)
    : ToolbarButton(this),
      connector_(connector),
      controller_(source_id, connector_, this),
      browser_(browser) {
  in_product_help_ = GlobalMediaControlsInProductHelpFactory::GetForProfile(
      browser_->profile());

  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::NOTIFY_ON_PRESS);
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_ICON_TOOLTIP_TEXT));

  ToolbarButton::Init();

  // We start hidden and only show once |controller_| tells us to.
  SetVisible(false);
}

MediaToolbarButtonView::~MediaToolbarButtonView() = default;

void MediaToolbarButtonView::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  if (MediaDialogView::IsShowing()) {
    MediaDialogView::HideDialog();
  } else {
    MediaDialogView::ShowDialog(this, &controller_, connector_);
    InformIPHOfDialogShown();
  }
}

void MediaToolbarButtonView::Show() {
  SetVisible(true);
  PreferredSizeChanged();
}

void MediaToolbarButtonView::Hide() {
  SetVisible(false);
  PreferredSizeChanged();
  InformIPHOfButtonDisabledorHidden();
}

void MediaToolbarButtonView::Enable() {
  SetEnabled(true);
  InformIPHOfButtonEnabled();
}

void MediaToolbarButtonView::Disable() {
  SetEnabled(false);
  InformIPHOfButtonDisabledorHidden();
}

SkColor MediaToolbarButtonView::GetInkDropBaseColor() const {
  return is_promo_showing_ ? GetNativeTheme()->GetSystemColor(
                                 ui::NativeTheme::kColorId_ProminentButtonColor)
                           : ToolbarButton::GetInkDropBaseColor();
}

void MediaToolbarButtonView::UpdateIcon() {
  // TODO(https://crbug.com/973500): Use actual icon instead of this
  // placeholder.
  const gfx::VectorIcon& icon = ::vector_icons::kPlayArrowIcon;

  // TODO(https://crbug.com/973500): When adding the actual icon, have the size
  // of the icon in the icon definition so we don't need to specify a size here.
  const int dip_size = 18;

  const SkColor normal_color =
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
  const SkColor disabled_color = GetThemeProvider()->GetColor(
      ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_INACTIVE);

  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(icon, dip_size, normal_color));
  SetImage(views::Button::STATE_DISABLED,
           gfx::CreateVectorIcon(icon, dip_size, disabled_color));
}

void MediaToolbarButtonView::ShowPromo() {
  GetPromoController().ShowPromo();
  is_promo_showing_ = true;
  GetInkDrop()->AnimateToState(views::InkDropState::ACTIVATED);
}

void MediaToolbarButtonView::OnPromoEnded() {
  is_promo_showing_ = false;
  GetInkDrop()->AnimateToState(views::InkDropState::HIDDEN);
}

GlobalMediaControlsPromoController&
MediaToolbarButtonView::GetPromoController() {
  if (!promo_controller_) {
    promo_controller_ = std::make_unique<GlobalMediaControlsPromoController>(
        this, browser_->profile());
  }
  return *promo_controller_;
}

void MediaToolbarButtonView::InformIPHOfDialogShown() {
  if (in_product_help_)
    in_product_help_->GlobalMediaControlsOpened();

  GetPromoController().OnMediaDialogOpened();
}

void MediaToolbarButtonView::InformIPHOfButtonEnabled() {
  if (in_product_help_)
    in_product_help_->ToolbarIconEnabled();
}

void MediaToolbarButtonView::InformIPHOfButtonDisabledorHidden() {
  if (in_product_help_)
    in_product_help_->ToolbarIconDisabled();

  GetPromoController().OnMediaToolbarButtonDisabledOrHidden();
}
