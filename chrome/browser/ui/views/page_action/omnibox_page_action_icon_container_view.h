// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_OMNIBOX_PAGE_ACTION_ICON_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_OMNIBOX_PAGE_ACTION_ICON_CONTAINER_VIEW_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/zoom/zoom_event_manager.h"
#include "components/zoom/zoom_event_manager_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/view.h"

class Browser;
class CommandUpdater;
class FindBarIcon;
class IntentPickerView;
class ManagePasswordsIconViews;
class PwaInstallView;
class TranslateIconView;
class ZoomView;

namespace send_tab_to_self {
class SendTabToSelfIconView;
}

class OmniboxPageActionIconContainerView
    : public views::View,
      public PageActionIconContainer,
      public zoom::ZoomEventManagerObserver {
 public:
  struct Params {
    Params();
    ~Params();

    std::vector<PageActionIconType> types_enabled;
    int icon_size = 0;
    SkColor icon_color = gfx::kPlaceholderColor;
    int between_icon_spacing = 0;
    Browser* browser = nullptr;
    CommandUpdater* command_updater = nullptr;
    PageActionIconView::Delegate* page_action_icon_delegate = nullptr;

   private:
    DISALLOW_COPY_AND_ASSIGN(Params);
  };

  explicit OmniboxPageActionIconContainerView(const Params& params);
  ~OmniboxPageActionIconContainerView() override;

  PageActionIconView* GetPageActionIconView(PageActionIconType type);

  // Updates the visual state of all enabled page action icons.
  void UpdateAll();

  // Activates the first visible but inactive icon for accessibility. Returns
  // whether any icons were activated.
  bool ActivateFirstInactiveBubbleForAccessibility();

  // Update the icons color.
  void SetIconColor(SkColor icon_color);

  // See comment in browser_window.h for more info.
  void ZoomChangedForActiveTab(bool can_show_bubble);

  // PageActionIconContainer:
  void UpdatePageActionIcon(PageActionIconType type) override;
  void ExecutePageActionIconForTesting(PageActionIconType type) override;

 private:
  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  // ZoomEventManagerObserver:
  // Updates the view for the zoom icon when default zoom levels change.
  void OnDefaultZoomLevelChanged() override;

  ZoomView* zoom_view_ = nullptr;
  FindBarIcon* find_bar_icon_ = nullptr;
  ManagePasswordsIconViews* manage_passwords_icon_ = nullptr;
  IntentPickerView* intent_picker_view_ = nullptr;
  PwaInstallView* pwa_install_view_ = nullptr;
  send_tab_to_self::SendTabToSelfIconView* send_tab_to_self_icon_view_ =
      nullptr;
  TranslateIconView* translate_icon_ = nullptr;
  std::vector<PageActionIconView*> page_action_icons_;

  ScopedObserver<zoom::ZoomEventManager, zoom::ZoomEventManagerObserver>
      zoom_observer_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPageActionIconContainerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_OMNIBOX_PAGE_ACTION_ICON_CONTAINER_VIEW_H_
