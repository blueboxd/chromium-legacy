// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_BASE_BUBBLE_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_BASE_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_button.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

class LoginBubbleHandler;

// Base bubble view for login screen bubbles.
class ASH_EXPORT LoginBaseBubbleView : public views::View,
                                       public ui::LayerAnimationObserver {
 public:
  // Without specifying a parent_window, the bubble will default to being in the
  // same container as anchor_view.
  explicit LoginBaseBubbleView(views::View* anchor_view);
  explicit LoginBaseBubbleView(views::View* anchor_view,
                               gfx::NativeView parent_window);
  ~LoginBaseBubbleView() override;

  void Show();
  void Hide();

  // Returns the button responsible for opening this bubble.
  virtual LoginButton* GetBubbleOpener() const;

  // Returns whether or not this bubble should show persistently.
  virtual bool IsPersistent() const;
  // Change the persistence of the bubble.
  virtual void SetPersistent(bool persistent);

  // Determine the position of the bubble prior to showing.
  virtual gfx::Point CalculatePosition();

  void SetAnchorView(views::View* anchor_view);
  views::View* GetAnchorView() const { return anchor_view_; }

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void OnBlur() override;

 protected:
  enum class PositioningStrategy {
    // Try to show bubble on the right side of the anchor, if there is no space
    // show on the left side.
    kShowOnRightSideOrLeftSide,
    // Try to show bubble on the left side of the anchor, if there is no space
    // show on the right side.
    kShowOnLeftSideOrRightSide,
  };
  // Returns calculated position using default positioning strategies.
  gfx::Point CalculatePositionUsingDefaultStrategy(PositioningStrategy strategy,
                                                   int horizontal_padding,
                                                   int vertical_padding) const;

  // Return area where bubble could be shown in.
  gfx::Rect GetBoundsAvailableToShowBubble() const;

 private:
  // Create a layer for this view if doesn't exist.
  void EnsureLayer();

  // Return bounds of the anchors root view. This bounds excludes virtual
  // keyboard.
  gfx::Rect GetRootViewBounds() const;
  // Return bounds of working area. This bounds excludes shelf.
  gfx::Rect GetWorkArea() const;
  void ScheduleAnimation(bool visible);

  views::View* anchor_view_;

  std::unique_ptr<LoginBubbleHandler> bubble_handler_;

  DISALLOW_COPY_AND_ASSIGN(LoginBaseBubbleView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_BASE_BUBBLE_VIEW_H_
