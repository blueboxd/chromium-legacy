// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"

MediaToolbarButtonView::MediaToolbarButtonView(
    service_manager::Connector* connector)
    : ToolbarButton(this),
      connector_(connector),
      controller_(connector_, this) {
  set_notify_action(Button::NOTIFY_ON_PRESS);
  EnableCanvasFlippingForRTLUI(false);
  ToolbarButton::Init();

  // We start hidden and only show once |controller_| tells us to.
  SetVisible(false);
}

MediaToolbarButtonView::~MediaToolbarButtonView() = default;

void MediaToolbarButtonView::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  if (MediaDialogView::IsShowing())
    MediaDialogView::HideDialog();
  else
    MediaDialogView::ShowDialog(this, connector_);
}

void MediaToolbarButtonView::Show() {
  SetVisible(true);
  PreferredSizeChanged();
}

void MediaToolbarButtonView::UpdateIcon() {
  // TODO(https://crbug.com/973500): Use actual icon instead of this
  // placeholder.
  const gfx::VectorIcon& icon = ::vector_icons::kPlayArrowIcon;

  // TODO(https://crbug.com/973500): When adding the actual icon, have the size
  // of the icon in the icon definition so we don't need to specify a size here.
  const int dip_size = 18;

  SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(icon, dip_size,
                            GetThemeProvider()->GetColor(
                                ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON)));
}
