// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WEBUI_TAB_STRIP_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WEBUI_TAB_STRIP_CONTAINER_VIEW_H_

#include <memory>
#include <set>

#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_embedder.h"
#include "chrome/common/buildflags.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if !BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#error
#endif

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

namespace ui {
class MenuModel;
}  // namespace ui

namespace views {
class MenuRunner;
class NativeViewHost;
class WebView;
}  // namespace views

class Browser;
class FeaturePromoBubbleView;

class WebUITabStripContainerView : public TabStripUIEmbedder,
                                   public gfx::AnimationDelegate,
                                   public views::AccessiblePaneView,
                                   public views::ButtonListener,
                                   public views::ViewObserver,
                                   public views::WidgetObserver {
 public:
  WebUITabStripContainerView(Browser* browser,
                             views::View* tab_contents_container,
                             views::View* drag_handle);
  ~WebUITabStripContainerView() override;

  static bool UseTouchableTabStrip();

  // For drag-and-drop support:
  static void GetDropFormatsForView(
      int* formats,
      std::set<ui::ClipboardFormatType>* format_types);
  static bool IsDraggedTab(const ui::OSExchangeData& data);

  void OpenForTabDrag();

  views::NativeViewHost* GetNativeViewHost();

  // Control buttons. Each must only be called once.
  std::unique_ptr<ToolbarButton> CreateNewTabButton();
  std::unique_ptr<views::View> CreateTabCounter();

  void UpdateButtons();

  // Should be called on BrowserView re-layout. If IPH is showing,
  // updates the promo for the new tab counter location.
  void UpdatePromoBubbleBounds();

  // Clicking the tab counter button opens and closes the container with
  // an animation, so it is unsuitable for an interactive test. This
  // should be called instead. View::SetVisible() isn't sufficient since
  // the container's preferred size will change.
  void SetVisibleForTesting(bool visible);
  views::WebView* web_view_for_testing() const { return web_view_; }
  ToolbarButton* new_tab_button_for_testing() const { return new_tab_button_; }
  views::View* tab_counter_for_testing() const { return tab_counter_; }

 private:
  class AutoCloser;
  class DragToOpenHandler;

  // Called as we are dragged open.
  void UpdateHeightForDragToOpen(float height_delta);

  enum class FlingDirection {
    kUp,
    kDown,
  };

  // Called when drag-to-open finishes. If |fling_direction| is present,
  // the user released their touch with a high velocity. We should use
  // just this direction to animate open or closed.
  void EndDragToOpen(
      base::Optional<FlingDirection> fling_direction = base::nullopt);

  void SetContainerTargetVisibility(bool target_visible);

  // When the container is open, it intercepts most tap and click
  // events. This checks if each event should be intercepted or passed
  // through to its target.
  bool EventShouldPropagate(const ui::Event& event);

  // Passed to the AutoCloser to handle closing.
  void CloseForEventOutsideTabStrip();

  // TabStripUI::Embedder:
  const ui::AcceleratorProvider* GetAcceleratorProvider() const override;
  void CloseContainer() override;
  void ShowContextMenuAtPoint(
      gfx::Point point,
      std::unique_ptr<ui::MenuModel> menu_model) override;
  void ShowEditDialogForGroupAtPoint(gfx::Point point,
                                     gfx::Rect rect,
                                     tab_groups::TabGroupId group) override;
  TabStripUILayout GetLayout() override;
  SkColor GetColor(int id) const override;

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  int GetHeightForWidth(int w) const override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(View* observed_view) override;
  void OnViewIsDeleting(View* observed_view) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // views::AccessiblePaneView
  bool SetPaneFocusAndFocusDefault() override;

  Browser* const browser_;
  views::WebView* const web_view_;
  views::View* tab_contents_container_;
  // TODO(1045669): Remove this, or run an experiment on bringing it back.
  ToolbarButton* new_tab_button_ = nullptr;
  views::View* tab_counter_ = nullptr;

  int desired_height_ = 0;
  base::Optional<float> current_drag_height_;

  // When opened, if currently open. Used to calculate metric for how
  // long the tab strip is kept open.
  base::Optional<base::TimeTicks> time_at_open_;

  feature_engagement::Tracker* const iph_tracker_;
  FeaturePromoBubbleView* tab_counter_promo_ = nullptr;

  gfx::SlideAnimation animation_{this};

  std::unique_ptr<AutoCloser> auto_closer_;
  std::unique_ptr<DragToOpenHandler> drag_to_open_handler_;

  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  std::unique_ptr<ui::MenuModel> context_menu_model_;

  ScopedObserver<views::View, views::ViewObserver> view_observer_{this};
  ScopedObserver<views::Widget, views::WidgetObserver> widget_observer_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WEBUI_TAB_STRIP_CONTAINER_VIEW_H_
