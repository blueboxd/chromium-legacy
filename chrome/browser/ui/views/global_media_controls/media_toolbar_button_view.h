// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller_delegate.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

// Media icon shown in the trusted area of toolbar. Its lifetime is tied to that
// of its parent ToolbarView. The icon is made visible when there is an active
// media session.
class MediaToolbarButtonView : public ToolbarButton,
                               public MediaToolbarButtonControllerDelegate,
                               public views::ButtonListener {
 public:
  explicit MediaToolbarButtonView(service_manager::Connector* connector);
  ~MediaToolbarButtonView() override;

  // MediaToolbarButtonControllerDelegate implementation.
  void Show() override;

  // views::ButtonListener implementation.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Updates the icon image.
  void UpdateIcon();

 private:
  service_manager::Connector* connector_;
  MediaToolbarButtonController controller_;

  DISALLOW_COPY_AND_ASSIGN(MediaToolbarButtonView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_VIEW_H_
