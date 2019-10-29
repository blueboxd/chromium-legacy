// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_ITEM_VIEW_H_
#define ASH_WM_OVERVIEW_OVERVIEW_ITEM_VIEW_H_

#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/window_mini_view.h"
#include "base/macros.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class ImageButton;
class View;
}  // namespace views

namespace ash {

// OverviewItemView covers the overview window and listens for events.
class ASH_EXPORT OverviewItemView
    : public WindowMiniView,
      public OverviewHighlightController::OverviewHighlightableView {
 public:
  // The visibility of the header. It may be fully visible or invisible, or
  // everything but the close button is visible.
  enum class HeaderVisibility {
    kInvisible,
    kCloseButtonInvisibleOnly,
    kVisible,
  };

  class EventDelegate {
   public:
    virtual void HandleMouseEvent(const ui::MouseEvent& event) = 0;
    virtual void HandleGestureEvent(ui::GestureEvent* event) = 0;
    virtual bool ShouldIgnoreGestureEvents() = 0;
    virtual void OnHighlightedViewActivated() = 0;
    virtual void OnHighlightedViewClosed() = 0;

   protected:
    virtual ~EventDelegate() {}
  };

  // If |show_preview| is true, this class will contain a child view which
  // mirrors |window|.
  OverviewItemView(EventDelegate* event_delegate,
                   aura::Window* window,
                   bool show_preview,
                   views::ImageButton* close_button);
  ~OverviewItemView() override;

  // Fades the app icon and title out if |visibility| is kInvisible, in
  // otherwise. If |close_button_| is not null, also fades the close button in
  // if |visibility| is kVisible, out otherwise. Sets
  // |current_header_visibility_| to |visibility|.
  void SetHeaderVisibility(HeaderVisibility visibility);

  // Hides the close button instantaneously, and then fades it in slowly and
  // with a long delay. Sets |current_header_visibility_| to kVisible. Assumes
  // that |close_button_| is not null, and that |current_header_visibility_| is
  // not kInvisible.
  void HideCloseInstantlyAndThenShowItSlowly();

  void ResetEventDelegate();

  // Refreshes |preview_view_| so that its content is up-to-date. Used by tab
  // dragging.
  void RefreshPreviewView();

  // OverviewHighlightController::OverviewHighlightableView:
  views::View* GetView() override;
  gfx::Rect GetHighlightBoundsInScreen() override;
  void MaybeActivateHighlightedView() override;
  void MaybeCloseHighlightedView() override;
  gfx::Point GetMagnifierFocusPointInScreen() override;

 protected:
  // views::View:
  const char* GetClassName() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool CanAcceptEvent(const ui::Event& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  // The delegate which all the events get forwarded to.
  EventDelegate* event_delegate_;

  views::ImageButton* close_button_;

  HeaderVisibility current_header_visibility_ = HeaderVisibility::kVisible;

  DISALLOW_COPY_AND_ASSIGN(OverviewItemView);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_ITEM_VIEW_H_
