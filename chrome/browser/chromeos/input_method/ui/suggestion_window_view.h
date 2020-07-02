// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_WINDOW_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_WINDOW_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_observer.h"

namespace ui {
namespace ime {

class AssistiveDelegate;
class SettingLinkView;
class SuggestionView;
struct AssistiveWindowButton;
struct SuggestionDetails;

const int kInvalid = -1;

// SuggestionWindowView is the main container of the suggestion window UI.
class UI_CHROMEOS_EXPORT SuggestionWindowView
    : public views::BubbleDialogDelegateView,
      public views::ButtonListener,
      public views::ButtonObserver {
 public:
  SuggestionWindowView(gfx::NativeView parent, AssistiveDelegate* delegate);
  ~SuggestionWindowView() override;
  views::Widget* InitWidget();

  // Hides.
  void Hide();

  // Shows suggestion text.
  void Show(const SuggestionDetails& details);

  void ShowMultipleCandidates(const std::vector<base::string16>& candidates);

  // This highlights/unhighlights a valid button based on the given params.
  // Only one button of the same id will be highlighted at anytime.
  void SetButtonHighlighted(const AssistiveWindowButton& button,
                            bool highlighted);

  void SetBounds(const gfx::Rect& cursor_bounds);

  views::View* GetCandidateAreaForTesting();

 private:
  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::ButtonObserver's override:
  void OnStateChanged(views::Button* observed_button,
                      views::Button::ButtonState old_state) override;

  void MaybeInitializeSuggestionViews(size_t candidates_size);

  void MakeVisible();

  // This highlights at most one candidate at any time.
  // No-op if index is out of range.
  void HighlightCandidate(int index);

  // This unhighlights the candidate at the given index.
  // No-op if the candidate is currently not highlighted or index is out of
  // range.
  void UnhighlightCandidate(int index);

  // views::BubbleDialogDelegateView:
  const char* GetClassName() const override;

  // The delegate to handle events from this class.
  AssistiveDelegate* delegate_;

  // The view containing all the suggestions.
  views::View* candidate_area_;

  // The view for rendering setting link, positioned below candidate_area_.
  SettingLinkView* setting_link_view_;

  // The items in view_
  std::vector<std::unique_ptr<SuggestionView>> candidate_views_;

  int highlighted_index_ = kInvalid;

  DISALLOW_COPY_AND_ASSIGN(SuggestionWindowView);
};

}  // namespace ime
}  // namespace ui

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_SUGGESTION_WINDOW_VIEW_H_
