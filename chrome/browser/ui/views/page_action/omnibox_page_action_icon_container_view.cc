// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/omnibox_page_action_icon_container_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/location_bar/find_bar_icon.h"
#include "chrome/browser/ui/views/location_bar/intent_picker_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/page_action/pwa_install_view.h"
#include "chrome/browser/ui/views/page_action/zoom_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_icon_view.h"
#include "chrome/browser/ui/views/translate/translate_icon_view.h"
#include "ui/views/layout/box_layout.h"

OmniboxPageActionIconContainerView::Params::Params() = default;
OmniboxPageActionIconContainerView::Params::~Params() = default;

OmniboxPageActionIconContainerView::OmniboxPageActionIconContainerView(
    const Params& params)
    : zoom_observer_(this) {
  DCHECK_GT(params.icon_size, 0);
  DCHECK_NE(params.icon_color, gfx::kPlaceholderColor);
  DCHECK(params.page_action_icon_delegate);

  views::BoxLayout& layout =
      *SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kHorizontal, gfx::Insets(),
          params.between_icon_spacing));
  // Right align to clip the leftmost items first when not enough space.
  layout.set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);

  for (PageActionIconType type : params.types_enabled) {
    switch (type) {
      case PageActionIconType::kFind:
        find_bar_icon_ =
            new FindBarIcon(params.browser, params.page_action_icon_delegate);
        page_action_icons_.push_back(find_bar_icon_);
        break;
      case PageActionIconType::kManagePasswords:
        DCHECK(params.command_updater);
        manage_passwords_icon_ = new ManagePasswordsIconViews(
            params.command_updater, params.page_action_icon_delegate);
        page_action_icons_.push_back(manage_passwords_icon_);
        break;
      case PageActionIconType::kIntentPicker:
        intent_picker_view_ = new IntentPickerView(
            params.browser, params.page_action_icon_delegate);
        page_action_icons_.push_back(intent_picker_view_);
        break;
      case PageActionIconType::kPwaInstall:
        DCHECK(params.command_updater);
        pwa_install_view_ = new PwaInstallView(
            params.command_updater, params.page_action_icon_delegate);
        page_action_icons_.push_back(pwa_install_view_);
        break;
      case PageActionIconType::kTranslate:
        DCHECK(params.command_updater);
        translate_icon_ = new TranslateIconView(
            params.command_updater, params.page_action_icon_delegate);
        page_action_icons_.push_back(translate_icon_);
        break;
      case PageActionIconType::kZoom:
        zoom_view_ = new ZoomView(params.page_action_icon_delegate);
        page_action_icons_.push_back(zoom_view_);
        break;
      case PageActionIconType::kSendTabToSelf:
        send_tab_to_self_icon_view_ =
            new send_tab_to_self::SendTabToSelfIconView(
                params.command_updater, params.page_action_icon_delegate);
        page_action_icons_.push_back(send_tab_to_self_icon_view_);
        break;
      case PageActionIconType::kLocalCardMigration:
      case PageActionIconType::kSaveCard:
        NOTREACHED();
        break;
    }
  }

  for (PageActionIconView* icon : page_action_icons_) {
    icon->SetVisible(false);
    icon->set_icon_size(params.icon_size);
    icon->SetIconColor(params.icon_color);
    AddChildView(icon);
  }

  if (params.browser) {
    zoom_observer_.Add(zoom::ZoomEventManager::GetForBrowserContext(
        params.browser->profile()));
  }
}

OmniboxPageActionIconContainerView::~OmniboxPageActionIconContainerView() {}

PageActionIconView* OmniboxPageActionIconContainerView::GetPageActionIconView(
    PageActionIconType type) {
  // TODO(https://crbug.com/788051): Update page action icons here as update
  // methods are migrated out of LocationBar to the PageActionIconContainer
  // interface.
  switch (type) {
    case PageActionIconType::kFind:
      return find_bar_icon_;
    case PageActionIconType::kManagePasswords:
      return manage_passwords_icon_;
    case PageActionIconType::kIntentPicker:
      return intent_picker_view_;
    case PageActionIconType::kPwaInstall:
      return pwa_install_view_;
    case PageActionIconType::kTranslate:
      return translate_icon_;
    case PageActionIconType::kZoom:
      return zoom_view_;
    case PageActionIconType::kSendTabToSelf:
      return send_tab_to_self_icon_view_;
    case PageActionIconType::kLocalCardMigration:
    case PageActionIconType::kSaveCard:
      NOTREACHED();
      return nullptr;
  }
  return nullptr;
}

void OmniboxPageActionIconContainerView::UpdateAll() {
  for (PageActionIconView* icon : page_action_icons_)
    icon->Update();
}

void OmniboxPageActionIconContainerView::UpdatePageActionIcon(
    PageActionIconType type) {
  PageActionIconView* icon = GetPageActionIconView(type);
  if (icon)
    icon->Update();
}

void OmniboxPageActionIconContainerView::ExecutePageActionIconForTesting(
    PageActionIconType type) {
  PageActionIconView* icon = GetPageActionIconView(type);
  if (icon)
    icon->ExecuteForTesting();
}

bool OmniboxPageActionIconContainerView::
    ActivateFirstInactiveBubbleForAccessibility() {
  for (PageActionIconView* icon : page_action_icons_) {
    if (!icon->GetVisible() || !icon->GetBubble())
      continue;

    views::Widget* widget = icon->GetBubble()->GetWidget();
    if (widget && widget->IsVisible() && !widget->IsActive()) {
      widget->Show();
      return true;
    }
  }
  return false;
}

void OmniboxPageActionIconContainerView::SetIconColor(SkColor icon_color) {
  for (PageActionIconView* icon : page_action_icons_)
    icon->SetIconColor(icon_color);
}

void OmniboxPageActionIconContainerView::ZoomChangedForActiveTab(
    bool can_show_bubble) {
  if (zoom_view_)
    zoom_view_->ZoomChangedForActiveTab(can_show_bubble);
}

void OmniboxPageActionIconContainerView::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void OmniboxPageActionIconContainerView::ChildVisibilityChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void OmniboxPageActionIconContainerView::OnDefaultZoomLevelChanged() {
  ZoomChangedForActiveTab(false);
}
