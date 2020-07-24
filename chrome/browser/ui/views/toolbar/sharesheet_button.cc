// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/sharesheet_button.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/paint_vector_icon.h"

SharesheetButton::SharesheetButton(Browser* browser)
    : ToolbarButton(this), browser_(browser) {}

SharesheetButton::~SharesheetButton() = default;

void SharesheetButton::UpdateIcon() {
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(
                    vector_icons::kHelpIcon,
                    GetThemeProvider()->GetColor(
                        ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON),
                    GetIconSize()));
}

void SharesheetButton::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  // On button press show sharesheet bubble.
  auto* profile = Profile::FromBrowserContext(
      browser_->tab_strip_model()->GetActiveWebContents()->GetBrowserContext());
  auto* sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(profile);
  sharesheet_service->ShowBubble(/* bubble_anchor_view */ this,
                                 /* intent */ nullptr);
}

int SharesheetButton::GetIconSize() const {
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  return (touch_ui && !browser_->app_controller()) ? kDefaultTouchableIconSize
                                                   : kDefaultIconSize;
}
