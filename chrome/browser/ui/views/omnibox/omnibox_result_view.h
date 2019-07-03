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
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

class OmniboxMatchCellView;
class OmniboxPopupContentsView;
class OmniboxTabSwitchButton;
enum class OmniboxPart;
enum class OmniboxPartState;
enum class OmniboxTint;

namespace gfx {
class Image;
}

namespace views {
class MenuRunner;
}

class OmniboxResultView : public views::View,
                          public views::AnimationDelegateViews,
                          public views::ButtonListener,
                          public views::ContextMenuController,
                          public ui::SimpleMenuModel::Delegate {
 public:
  OmniboxResultView(OmniboxPopupContentsView* popup_contents_view,
                    int model_index);
  ~OmniboxResultView() override;

  // Helper to get the color for |part| using the current state and tint.
  SkColor GetColor(OmniboxPart part) const;

  // Updates the match used to paint the contents of this result view. We copy
  // the match so that we can continue to paint the last result even after the
  // model has changed.
  void SetMatch(const AutocompleteMatch& match);

  void ShowKeyword(bool show_keyword);

  void Invalidate(bool force_reapply_styles = false);

  // Invoked when this result view has been selected.
  void OnSelected();

  // Whether |this| matches the model's selected index.
  bool IsSelected() const;

  OmniboxPartState GetThemeState() const;
  OmniboxTint CalculateTint() const;

  // Notification that the match icon has changed and schedules a repaint.
  void OnMatchIconUpdated();

  // Stores the image in a local data member and schedules a repaint.
  void SetRichSuggestionImage(const gfx::ImageSkia& image);

  // views::ButtonListener:

  // Called when tab switch button pressed, due to being a listener.
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
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsCommandIdVisible(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // Returns the height of the text portion of the result view.
  int GetTextHeight() const;

  gfx::Image GetIcon() const;

  // Sets the hovered state of this result.
  void SetHovered(bool hovered);

  // Call model's OpenMatch() with the selected index and provided disposition
  // and timestamp the match was selected (base::TimeTicks() if unknown).
  void OpenMatch(WindowOpenDisposition disposition,
                 base::TimeTicks match_selection_timestamp);

  // views::View:
  const char* GetClassName() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // The parent view.
  OmniboxPopupContentsView* const popup_contents_view_;

  // This result's model index.
  size_t model_index_;

  // Whether this view is in the hovered state.
  bool is_hovered_;

  // The data this class is built to display (the "Omnibox Result").
  AutocompleteMatch match_;

  // Accessible name (enables to emit certain events).
  base::string16 accessible_name_;

  // For sliding in the keyword search.
  std::unique_ptr<gfx::SlideAnimation> animation_;

  // Context menu related members.
  ui::SimpleMenuModel context_menu_contents_{this};
  std::unique_ptr<views::MenuRunner> context_menu_runner_;

  // Weak pointers for easy reference.
  OmniboxMatchCellView* suggestion_view_;  // The leading (or left) view.
  OmniboxMatchCellView* keyword_view_;     // The trailing (or right) view.
  std::unique_ptr<OmniboxTabSwitchButton> suggestion_tab_switch_button_;

  base::WeakPtrFactory<OmniboxResultView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OmniboxResultView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_
