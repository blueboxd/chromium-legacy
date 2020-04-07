// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTROLLER_VIEWS_H_

#include <string>

#include "chrome/browser/ui/caption_bubble_controller.h"

class Browser;

namespace views {
class Widget;
}

namespace captions {

class CaptionBubble;

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble Controller for Views
//
//  The implementation of the caption bubble controller for Views.
//
class CaptionBubbleControllerViews : public CaptionBubbleController {
 public:
  explicit CaptionBubbleControllerViews(Browser* browser);
  ~CaptionBubbleControllerViews() override;
  CaptionBubbleControllerViews(const CaptionBubbleControllerViews&) = delete;
  CaptionBubbleControllerViews& operator=(const CaptionBubbleControllerViews&) =
      delete;

  // Called when captions are received from the service.
  void OnCaptionReceived(const std::string& text) override;

  // Called when the active tab changes.
  void OnActiveTabChanged(int index) override;

 private:
  // A callback passed to the CaptionBubble which is called when the
  // CaptionBubble is destroyed.
  void OnCaptionBubbleDestroyed();

  CaptionBubble* caption_bubble_;
  views::Widget* caption_widget_;
};

}  // namespace captions

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTROLLER_VIEWS_H_
