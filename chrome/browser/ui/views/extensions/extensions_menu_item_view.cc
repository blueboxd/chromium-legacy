// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "ui/views/layout/fill_layout.h"

ExtensionsMenuItemView::ExtensionsMenuItemView(
    Browser* browser,
    std::unique_ptr<ToolbarActionViewController> controller) {
  extensions_menu_button_ = AddChildView(
      std::make_unique<ExtensionsMenuButton>(browser, std::move(controller)));
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

ExtensionsMenuItemView::~ExtensionsMenuItemView() = default;

void ExtensionsMenuItemView::UpdatePinButton() {
  extensions_menu_button_->UpdatePinButton();
}

views::ButtonController*
ExtensionsMenuItemView::primary_action_button_controller_for_testing() {
  return extensions_menu_button_->button_controller();
}
