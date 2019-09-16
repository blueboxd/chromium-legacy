// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TOOLBAR_BUTTON_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TOOLBAR_BUTTON_PROVIDER_H_

#include "chrome/browser/ui/page_action/page_action_icon_type.h"

class AppMenuButton;
class BrowserActionsContainer;
class PageActionIconView;
class ToolbarActionView;

namespace gfx {
class Rect;
}

namespace views {
class AccessiblePaneView;
class View;
}

// An interface implemented by a view contains and provides access to toolbar
// buttons in a BrowserView.
class ToolbarButtonProvider {
 public:
  // Gets the browser actions container.
  // TODO(pbos): Transition callers off of this function.
  virtual BrowserActionsContainer* GetBrowserActionsContainer() = 0;

  // Gets the associated ToolbarActionView for this id.
  virtual ToolbarActionView* GetToolbarActionViewForId(
      const std::string& id) = 0;

  // Gets the default view to use as an anchor for extension dialogs if the
  // ToolbarActionView is not visible or available.
  virtual views::View* GetDefaultExtensionDialogAnchorView() = 0;

  // Gets the specified page action icon.
  virtual PageActionIconView* GetPageActionIconView(
      PageActionIconType type) = 0;

  // Gets the app menu button.
  virtual AppMenuButton* GetAppMenuButton() = 0;

  // Gets the area available for the find bar in widget space where
  // |contents_height| is the amount of vertical space available, otherwise if
  // there is no appropriate anchor point returns empty gfx::Rect.
  virtual gfx::Rect GetFindBarBoundingBox(int contents_height) const = 0;

  // Gives the toolbar focus.
  virtual void FocusToolbar() = 0;

  // Returns the toolbar as an AccessiblePaneView.
  virtual views::AccessiblePaneView* GetAsAccessiblePaneView() = 0;

  // Returns the appropriate anchor view for the page action icon.
  virtual views::View* GetAnchorView(PageActionIconType type) = 0;

  // See comment in browser_window.h for more info.
  virtual void ZoomChangedForActiveTab(bool can_show_bubble) = 0;

  // TODO(calamity): Move other buttons and button actions into here.
 protected:
  virtual ~ToolbarButtonProvider() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TOOLBAR_BUTTON_PROVIDER_H_
