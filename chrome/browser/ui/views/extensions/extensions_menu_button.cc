// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"

#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/layout/box_layout.h"

const char ExtensionsMenuButton::kClassName[] = "ExtensionsMenuButton";

ExtensionsMenuButton::ExtensionsMenuButton(
    Browser* browser,
    ExtensionsMenuItemView* parent,
    ToolbarActionViewController* controller)
    : HoverButton(this,
                  ExtensionsMenuView::CreateFixedSizeIconView(),
                  base::string16(),
                  base::string16(),
                  std::make_unique<views::View>(),
                  true,
                  true),
      browser_(browser),
      parent_(parent),
      controller_(controller) {
  set_auto_compute_tooltip(false);
  controller_->SetDelegate(this);
  UpdateState();
}

ExtensionsMenuButton::~ExtensionsMenuButton() = default;

const char* ExtensionsMenuButton::GetClassName() const {
  return kClassName;
}

void ExtensionsMenuButton::ButtonPressed(Button* sender,
                                         const ui::Event& event) {
  controller_->ExecuteAction(true);
}

// ToolbarActionViewDelegateViews:
views::View* ExtensionsMenuButton::GetAsView() {
  return this;
}

views::FocusManager* ExtensionsMenuButton::GetFocusManagerForAccelerator() {
  return GetFocusManager();
}

views::Button* ExtensionsMenuButton::GetReferenceButtonForPopup() {
  return BrowserView::GetBrowserViewForBrowser(browser_)
      ->toolbar()
      ->GetExtensionsButton();
}

content::WebContents* ExtensionsMenuButton::GetCurrentWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void ExtensionsMenuButton::UpdateState() {
  DCHECK_EQ(views::ImageView::kViewClassName, icon_view()->GetClassName());
  static_cast<views::ImageView*>(icon_view())
      ->SetImage(controller_
                     ->GetIcon(GetCurrentWebContents(),
                               icon_view()->GetPreferredSize())
                     .AsImageSkia());
  SetTitleTextWithHintRange(controller_->GetActionName(),
                            gfx::Range::InvalidRange());
  SetTooltipText(controller_->GetTooltip(GetCurrentWebContents()));
  SetEnabled(controller_->IsEnabled(GetCurrentWebContents()));
}

bool ExtensionsMenuButton::IsMenuRunning() const {
  return parent_->IsContextMenuRunning();
}
