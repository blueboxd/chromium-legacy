// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_LINUX_BROWSER_FRAME_VIEW_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_LINUX_BROWSER_FRAME_VIEW_LAYOUT_H_

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"
#include "ui/views/linux_ui/nav_button_provider.h"

// A specialization of OpaqueBrowserFrameViewLayout that is also able
// to layout frame buttons that were rendered by GTK.
class DesktopLinuxBrowserFrameViewLayout : public OpaqueBrowserFrameViewLayout {
 public:
  DesktopLinuxBrowserFrameViewLayout(
      views::NavButtonProvider* nav_button_provider);

 protected:
  // OpaqueBrowserFrameViewLayout:
  int CaptionButtonY(views::FrameButton button_id,
                     bool restored) const override;
  TopAreaPadding GetTopAreaPadding(bool has_leading_buttons,
                                   bool has_trailing_buttons) const override;
  int GetWindowCaptionSpacing(views::FrameButton button_id,
                              bool leading_spacing,
                              bool is_leading_button) const override;

 private:
  // Converts a FrameButton to a FrameButtonDisplayType, taking into
  // consideration the maximized state of the browser window.
  views::NavButtonProvider::FrameButtonDisplayType GetButtonDisplayType(
      views::FrameButton button_id) const;

  views::NavButtonProvider* nav_button_provider_;

  DISALLOW_COPY_AND_ASSIGN(DesktopLinuxBrowserFrameViewLayout);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_LINUX_BROWSER_FRAME_VIEW_LAYOUT_H_
