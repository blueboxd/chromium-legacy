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

  // Called when a transcription is received from the service.
  void OnTranscription(const chrome::mojom::TranscriptionResultPtr&
                           transcription_result) override;

  // Called when the active tab changes.
  void OnActiveTabChanged(int index) override;

  // Called when the caption style changes.
  void UpdateCaptionStyle(
      base::Optional<ui::CaptionStyle> caption_style) override;

 private:
  friend class CaptionBubbleControllerViewsTest;
  // A callback passed to the CaptionBubble which is called when the
  // CaptionBubble is destroyed.
  void OnCaptionBubbleDestroyed();

  CaptionBubble* caption_bubble_;
  views::Widget* caption_widget_;

  std::string final_text_;
};

}  // namespace captions

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTROLLER_VIEWS_H_
