// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_page_action_icon_container_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_icon_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

ToolbarPageActionIconContainerView::ToolbarPageActionIconContainerView(
    Browser* browser)
    : ToolbarIconContainerView(
          /*uses_highlight=*/!browser->profile()->IsIncognitoProfile()),
      browser_(browser) {
  manage_passwords_icon_views_ =
      new ManagePasswordsIconViews(browser->command_controller(), this);
  page_action_icons_.push_back(manage_passwords_icon_views_);

  local_card_migration_icon_view_ = new autofill::LocalCardMigrationIconView(
      browser->command_controller(), browser, this,
      // TODO(crbug.com/932818): The font list and the icon color may not be
      // what we want here. Put placeholders for now.
      views::style::GetFont(CONTEXT_TOOLBAR_BUTTON,
                            views::style::STYLE_PRIMARY));
  page_action_icons_.push_back(local_card_migration_icon_view_);

  save_card_icon_view_ = new autofill::SaveCardIconView(
      browser->command_controller(), browser, this,
      // TODO(crbug.com/932818): The font list and the icon color may not be
      // what we want here. Put placeholders for now.
      views::style::GetFont(CONTEXT_TOOLBAR_BUTTON,
                            views::style::STYLE_PRIMARY));
  page_action_icons_.push_back(save_card_icon_view_);

  for (PageActionIconView* icon_view : page_action_icons_) {
    icon_view->SetVisible(false);
    icon_view->AddButtonObserver(this);
    icon_view->views::View::AddObserver(this);
    AddChildView(icon_view);
  }

  avatar_ = new AvatarToolbarButton(browser);
  AddMainButton(avatar_);
}

ToolbarPageActionIconContainerView::~ToolbarPageActionIconContainerView() {
  for (PageActionIconView* icon_view : page_action_icons_) {
    icon_view->RemoveButtonObserver(this);
    icon_view->views::View::RemoveObserver(this);
  }
}

void ToolbarPageActionIconContainerView::UpdateAllIcons() {
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  const SkColor icon_color =
      theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);

  for (PageActionIconView* icon_view : page_action_icons_) {
    icon_view->SetIconColor(icon_color);
    icon_view->Update();
  }

  if (avatar_)
    avatar_->UpdateIcon();

  UpdateAvatarIconStateUi();
}

PageActionIconView* ToolbarPageActionIconContainerView::GetIconView(
    PageActionIconType icon_type) {
  switch (icon_type) {
    case PageActionIconType::kLocalCardMigration:
      return local_card_migration_icon_view_;
    case PageActionIconType::kSaveCard:
      return save_card_icon_view_;
    case PageActionIconType::kManagePasswords:
      return manage_passwords_icon_views_;
    default:
      NOTREACHED();
      return nullptr;
  }
}

bool ToolbarPageActionIconContainerView::
    ActivateFirstInactiveBubbleForAccessibility() {
  for (PageActionIconView* icon_view : page_action_icons_) {
    if (FocusInactiveBubbleForIcon(icon_view))
      return true;
  }
  return false;
}

void ToolbarPageActionIconContainerView::UpdatePageActionIcon(
    PageActionIconType icon_type) {
  PageActionIconView* icon = GetIconView(icon_type);
  if (icon)
    icon->Update();

  UpdateAvatarIconStateUi();
}

void ToolbarPageActionIconContainerView::ExecutePageActionIconForTesting(
    PageActionIconType type) {
  PageActionIconView* icon = GetIconView(type);
  if (icon)
    icon->ExecuteForTesting();
}

SkColor ToolbarPageActionIconContainerView::GetPageActionInkDropColor() const {
  return GetToolbarInkDropBaseColor(this);
}

content::WebContents*
ToolbarPageActionIconContainerView::GetWebContentsForPageActionIconView() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void ToolbarPageActionIconContainerView::OnThemeChanged() {
  // Update icon color.
  UpdateAllIcons();
}

bool ToolbarPageActionIconContainerView::FocusInactiveBubbleForIcon(
    PageActionIconView* icon_view) {
  if (!icon_view->GetVisible() || !icon_view->GetBubble())
    return false;

  views::Widget* widget = icon_view->GetBubble()->GetWidget();
  if (widget && widget->IsVisible() && !widget->IsActive()) {
    widget->Show();
    return true;
  }
  return false;
}

void ToolbarPageActionIconContainerView::UpdateAvatarIconStateUi() {
  // If it is in Incognito window, the avatar button shows a text "Incognito"
  // which should not be updated in any case.
  if (browser_->profile()->IsIncognitoProfile())
    return;

  bool suppress_avatar_button_state = false;
  for (PageActionIconView* icon_view : page_action_icons_) {
    if (icon_view->GetVisible()) {
      suppress_avatar_button_state = true;
      break;
    }
  }
  avatar_->SetSuppressAvatarButtonState(suppress_avatar_button_state);
}
