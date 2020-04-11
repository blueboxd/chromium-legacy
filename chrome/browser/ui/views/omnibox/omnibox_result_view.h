// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

class OmniboxMatchCellView;
class OmniboxPopupContentsView;
class OmniboxSuggestionButtonRowView;
class OmniboxTabSwitchButton;
enum class OmniboxPart;
enum class OmniboxPartState;

namespace gfx {
class Image;
}

namespace views {
class Button;
class FocusRing;
}  // namespace views

class OmniboxResultView : public views::View,
                          public views::AnimationDelegateViews,
                          public views::ButtonListener {
 public:
  OmniboxResultView(OmniboxPopupContentsView* popup_contents_view,
                    size_t model_index);
  ~OmniboxResultView() override;

  // Static method to share logic about how to set backgrounds of popup cells.
  static std::unique_ptr<views::Background> GetPopupCellBackground(
      views::View* view,
      OmniboxPartState part_state);

  // Helper to get the color for |part| using the current state.
  SkColor GetColor(OmniboxPart part) const;

  // Updates the match used to paint the contents of this result view. We copy
  // the match so that we can continue to paint the last result even after the
  // model has changed.
  void SetMatch(const AutocompleteMatch& match);

  void ShowKeyword(bool show_keyword);

  // Applies the current theme to the current text and widget colors.
  // Also refreshes the icons which may need to be re-colored as well.
  void ApplyThemeAndRefreshIcons(bool force_reapply_styles = false);

  // Invoked when this result view has been selected or unselected.
  void OnSelectionStateChanged();

  // Whether |this| matches the model's selected index.
  bool IsSelected() const;

  // Returns the visible (and keyboard-focusable) secondary button, or nullptr
  // if none exists for this suggestion.
  views::Button* GetSecondaryButton();

  // If this view has a secondary button, triggers the action and returns true.
  bool MaybeTriggerSecondaryButton(const ui::Event& event);

  // This returns the accessibility label for this result view. This is an
  // extended version of AutocompleteMatchType::ToAccessibilityLabel() which
  // also returns narration about the secondary button.
  base::string16 ToAccessibilityLabelWithSecondaryButton(
      const base::string16& match_text,
      size_t total_matches,
      int* label_prefix_length = nullptr);

  OmniboxPartState GetThemeState() const;

  // Notification that the match icon has changed and schedules a repaint.
  void OnMatchIconUpdated();

  // Stores the image in a local data member and schedules a repaint.
  void SetRichSuggestionImage(const gfx::ImageSkia& image);

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Called to indicate tab switch button has been focused.
  void ProvideButtonFocusHint();

  // Removes the shown |match_| from history, if possible.
  void RemoveSuggestion() const;

  // Helper to emit accessibility events (may only emit if conditions are met).
  void EmitTextChangedAccessiblityEvent();
  void EmitSelectedChildrenChangedAccessibilityEvent();

  // views::View:
  void Layout() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;

 private:
  // Returns the height of the text portion of the result view.
  int GetTextHeight() const;

  gfx::Image GetIcon() const;

  // Updates the highlight state of the row, as well as conditionally shows
  // controls that are only visible on row hover.
  void UpdateHoverState();

  // Call model's OpenMatch() with the selected index and provided disposition
  // and timestamp the match was selected (base::TimeTicks() if unknown).
  void OpenMatch(WindowOpenDisposition disposition,
                 base::TimeTicks match_selection_timestamp);

  // Sets the visibility of the |remove_suggestion_button_| based on the current
  // state.
  void UpdateRemoveSuggestionVisibility();

  // views::View:
  const char* GetClassName() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // The parent view.
  OmniboxPopupContentsView* const popup_contents_view_;

  // This result's model index.
  size_t model_index_;

  // The data this class is built to display (the "Omnibox Result").
  AutocompleteMatch match_;

  // Accessible name (enables to emit certain events).
  base::string16 accessible_name_;

  // For sliding in the keyword search.
  std::unique_ptr<gfx::SlideAnimation> animation_;

  // Weak pointers for easy reference.
  OmniboxMatchCellView* suggestion_view_;  // The leading (or left) view.
  OmniboxMatchCellView* keyword_view_;     // The trailing (or right) view.
  OmniboxTabSwitchButton* suggestion_tab_switch_button_;

  // The row of buttons, only assigned and used if OmniboxSuggestionButtonRow
  // feature is enabled. It is owned by the base view, not this raw pointer.
  OmniboxSuggestionButtonRowView* button_row_ = nullptr;

  // The "X" button at the end of the match cell, used to remove suggestions.
  views::ImageButton* remove_suggestion_button_;
  std::unique_ptr<views::FocusRing> remove_suggestion_focus_ring_;

  base::WeakPtrFactory<OmniboxResultView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OmniboxResultView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_
