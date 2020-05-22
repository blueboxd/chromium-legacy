// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTROLLER_VIEWS_H_

#include <string>
#include <unordered_map>

#include "chrome/browser/ui/caption_bubble_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

namespace views {
class View;
class Widget;
}

namespace captions {

class CaptionBubble;

struct CaptionText {
  std::string final_text;
  std::string partial_text;

  std::string full_text() { return final_text + partial_text; }
};

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble Controller for Views
//
//  The implementation of the caption bubble controller for Views.
//
class CaptionBubbleControllerViews : public CaptionBubbleController,
                                     public TabStripModelObserver {
 public:
  static views::View* GetCaptionBubbleAccessiblePane(Browser* browser);

  explicit CaptionBubbleControllerViews(Browser* browser);
  ~CaptionBubbleControllerViews() override;
  CaptionBubbleControllerViews(const CaptionBubbleControllerViews&) = delete;
  CaptionBubbleControllerViews& operator=(const CaptionBubbleControllerViews&) =
      delete;

  // Called when a transcription is received from the service.
  void OnTranscription(
      const chrome::mojom::TranscriptionResultPtr& transcription_result,
      content::WebContents* web_contents) override;

  // Called when the caption style changes.
  void UpdateCaptionStyle(
      base::Optional<ui::CaptionStyle> caption_style) override;

  // Returns the view of the caption bubble which should receive focus, if one
  // exists.
  views::View* GetFocusableCaptionBubble();

 private:
  friend class CaptionBubbleControllerViewsTest;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // A callback passed to the CaptionBubble which is called when the
  // CaptionBubble is destroyed.
  void OnCaptionBubbleDestroyed();

  // Sets the caption bubble text to the text belonging to the active tab.
  void SetCaptionBubbleText();

  CaptionBubble* caption_bubble_;
  views::Widget* caption_widget_;
  Browser* browser_;

  // The web contents corresponding to the active tab.
  content::WebContents* active_contents_;

  // Stores web contents that have received transcription, mapped to their
  // corresponding caption bubble texts. Store the partial texts as well as the
  // final texts in order to show the latest partial text to a user when they
  // switch back to the tab in case the speech service has not sent a final
  // transcription in a while.
  std::unordered_map<content::WebContents*, CaptionText> caption_texts_;
};
}  // namespace captions

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTROLLER_VIEWS_H_
