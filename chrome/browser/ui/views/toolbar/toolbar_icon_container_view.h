// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ICON_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ICON_CONTAINER_VIEW_H_

#include "base/macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_observer.h"
#include "ui/views/view.h"

// A general view container for any type of toolbar icons.
class ToolbarIconContainerView : public views::View,
                                 public gfx::AnimationDelegate,
                                 public views::ButtonObserver,
                                 public views::ViewObserver {
 public:
  explicit ToolbarIconContainerView(bool uses_highlight);
  ~ToolbarIconContainerView() override;

  // Update all the icons it contains. Override by subclass.
  virtual void UpdateAllIcons();

  // Adds the RHS child as well as setting its margins.
  void AddMainButton(views::Button* main_button);

  // views::ButtonObserver:
  void OnHighlightChanged(views::Button* observed_button,
                          bool highlighted) override;
  void OnStateChanged(views::Button* observed_button,
                      views::Button::ButtonState old_state) override;

  // views::ViewObserver:
  void OnViewFocused(views::View* observed_view) override;
  void OnViewBlurred(views::View* observed_view) override;

  bool uses_highlight() { return uses_highlight_; }

 private:
  friend class ToolbarPageActionIconContainerViewBrowserTest;

  // views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  gfx::Insets GetInsets() const override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  bool ShouldDisplayHighlight();
  void UpdateHighlight();
  void SetHighlightBorder();

  // Determine whether the container shows its highlight border.
  const bool uses_highlight_;

  // The main view is nominally always present and is last child in the view
  // hierarchy.
  views::Button* main_button_ = nullptr;

  // Points to the child view that is currently highlighted.
  views::Button* highlighted_button_ = nullptr;

  // Fade-in/out animation for the highlight border.
  gfx::SlideAnimation highlight_animation_{this};

  DISALLOW_COPY_AND_ASSIGN(ToolbarIconContainerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ICON_CONTAINER_VIEW_H_
