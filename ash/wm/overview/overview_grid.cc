// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_grid.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "ash/kiosk_next/kiosk_next_shell_controller_impl.h"
#include "ash/metrics/histogram_macros.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/fps_counter.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/cleanup_animation_observer.h"
#include "ash/wm/overview/drop_target_view.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_delegate.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/rounded_rect_view.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/i18n/string_search.h"
#include "base/numerics/ranges.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"

namespace ash {
namespace {

// The color and opacity of the overview selector.
constexpr SkColor kWindowSelectionColor = SkColorSetARGB(36, 255, 255, 255);

// Corner radius and shadow applied to the overview selector border.
constexpr int kWindowSelectionRadius = 9;
constexpr int kWindowSelectionShadowElevation = 24;

// Windows are not allowed to get taller than this.
constexpr int kMaxHeight = 512;

// Margins reserved in the overview mode.
constexpr float kOverviewInsetRatio = 0.05f;

// Additional vertical inset reserved for windows in overview mode.
constexpr float kOverviewVerticalInset = 0.1f;

// Histogram names for overview enter/exit smoothness in clamshell,
// tablet mode and splitview.
constexpr char kOverviewEnterClamshellHistogram[] =
    "Ash.Overview.AnimationSmoothness.Enter.ClamshellMode";
constexpr char kOverviewEnterSingleClamshellHistogram[] =
    "Ash.Overview.AnimationSmoothness.Enter.SingleClamshellMode";
constexpr char kOverviewEnterTabletHistogram[] =
    "Ash.Overview.AnimationSmoothness.Enter.TabletMode";
constexpr char kOverviewEnterSplitViewHistogram[] =
    "Ash.Overview.AnimationSmoothness.Enter.SplitView";

constexpr char kOverviewExitClamshellHistogram[] =
    "Ash.Overview.AnimationSmoothness.Exit.ClamshellMode";
constexpr char kOverviewExitSingleClamshellHistogram[] =
    "Ash.Overview.AnimationSmoothness.Exit.SingleClamshellMode";
constexpr char kOverviewExitTabletHistogram[] =
    "Ash.Overview.AnimationSmoothness.Exit.TabletMode";
constexpr char kOverviewExitSplitViewHistogram[] =
    "Ash.Overview.AnimationSmoothness.Exit.SplitView";

// Returns the vector for the fade in animation.
gfx::Vector2d GetSlideVectorForFadeIn(OverviewSession::Direction direction,
                                      const gfx::Rect& bounds) {
  gfx::Vector2d vector;
  switch (direction) {
    case OverviewSession::UP:
    case OverviewSession::LEFT:
      vector.set_x(-bounds.width());
      break;
    case OverviewSession::DOWN:
    case OverviewSession::RIGHT:
      vector.set_x(bounds.width());
      break;
  }
  return vector;
}

template <const char* clamshell_single_name,
          const char* clamshell_multi_name,
          const char* tablet_name,
          const char* splitview_name>
class OverviewFpsCounter : public FpsCounter {
 public:
  OverviewFpsCounter(ui::Compositor* compositor,
                     bool single_animation_in_clamshell)
      : FpsCounter(compositor),
        single_animation_in_clamshell_(single_animation_in_clamshell) {}
  ~OverviewFpsCounter() override {
    int smoothness = ComputeSmoothness();
    if (smoothness < 0)
      return;
    if (single_animation_in_clamshell_)
      UMA_HISTOGRAM_PERCENTAGE_IN_CLAMSHELL(clamshell_single_name, smoothness);
    else
      UMA_HISTOGRAM_PERCENTAGE_IN_CLAMSHELL(clamshell_multi_name, smoothness);
    UMA_HISTOGRAM_PERCENTAGE_IN_TABLET_NON_SPLITVIEW(tablet_name, smoothness);
    UMA_HISTOGRAM_PERCENTAGE_IN_SPLITVIEW(splitview_name, smoothness);
  }

 private:
  // True if only top window animates upon enter/exit overview in clamshell.
  bool single_animation_in_clamshell_;

  DISALLOW_COPY_AND_ASSIGN(OverviewFpsCounter);
};

using OverviewEnterFpsCounter =
    OverviewFpsCounter<kOverviewEnterSingleClamshellHistogram,
                       kOverviewEnterClamshellHistogram,
                       kOverviewEnterTabletHistogram,
                       kOverviewEnterSplitViewHistogram>;
using OverviewExitFpsCounter =
    OverviewFpsCounter<kOverviewExitSingleClamshellHistogram,
                       kOverviewExitClamshellHistogram,
                       kOverviewExitTabletHistogram,
                       kOverviewExitSplitViewHistogram>;

class ShutdownAnimationFpsCounterObserver : public OverviewObserver {
 public:
  ShutdownAnimationFpsCounterObserver(ui::Compositor* compositor,
                                      bool single_animation)
      : fps_counter_(compositor, single_animation) {
    Shell::Get()->overview_controller()->AddObserver(this);
  }
  ~ShutdownAnimationFpsCounterObserver() override {
    Shell::Get()->overview_controller()->RemoveObserver(this);
  }

  // OverviewObserver:
  void OnOverviewModeEndingAnimationComplete(bool canceled) override {
    delete this;
  }

 private:
  OverviewExitFpsCounter fps_counter_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownAnimationFpsCounterObserver);
};

// Creates |drop_target_widget_|. It's created when a window or overview item is
// dragged around, and destroyed when the drag ends. If |animate| is true, then
// the drop target will fade in.
std::unique_ptr<views::Widget> CreateDropTargetWidget(
    aura::Window* dragged_window,
    bool animate) {
  aura::Window* parent = dragged_window->parent();
  gfx::Rect bounds = dragged_window->bounds();
  ::wm::ConvertRectToScreen(parent, &bounds);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.activatable = views::Widget::InitParams::Activatable::ACTIVATABLE_NO;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = false;
  params.parent = parent;
  params.bounds = bounds;
  auto widget = std::make_unique<views::Widget>();
  widget->set_focus_on_creation(false);
  widget->Init(params);

  // Show plus icon if drag a tab from a multi-tab window.
  widget->SetContentsView(new DropTargetView(
      dragged_window->GetProperty(ash::kTabDraggingSourceWindowKey)));
  aura::Window* drop_target_window = widget->GetNativeWindow();
  drop_target_window->SetProperty(kHideInDeskMiniViewKey, true);
  drop_target_window->parent()->StackChildAtBottom(drop_target_window);
  widget->Show();

  if (animate) {
    widget->SetOpacity(0.f);
    ScopedOverviewAnimationSettings settings(
        OVERVIEW_ANIMATION_DROP_TARGET_FADE_IN, drop_target_window);
    widget->SetOpacity(1.f);
  } else {
    widget->SetOpacity(1.f);
  }
  return widget;
}

// Gets the expected grid bounds according to current splitview state.
gfx::Rect GetGridBoundsInScreenAfterDragging(aura::Window* dragged_window) {
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  switch (split_view_controller->state()) {
    case SplitViewState::kLeftSnapped:
      return split_view_controller->GetSnappedWindowBoundsInScreen(
          dragged_window, SplitViewController::RIGHT);
    case SplitViewState::kRightSnapped:
      return split_view_controller->GetSnappedWindowBoundsInScreen(
          dragged_window, SplitViewController::LEFT);
    default:
      return screen_util::
          GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
              dragged_window);
  }
}

// When split view mode is not active, the grid bounds are updated according to
// |indicator_state|, to achieve the effect that the overview windows get out of
// the way of a split view drag indicator when it expands into a preview area.
// When split view mode is active, instead of keeping the overview windows away
// from the preview area, they are kept away from the already snapped window (by
// just forwarding |dragged_window| to GetGridBoundsInScreenAfterDragging()).
gfx::Rect GetGridBoundsInScreenDuringDragging(aura::Window* dragged_window,
                                              IndicatorState indicator_state) {
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  if (split_view_controller->InSplitViewMode())
    return GetGridBoundsInScreenAfterDragging(dragged_window);
  switch (indicator_state) {
    case IndicatorState::kPreviewAreaLeft:
      return split_view_controller->GetSnappedWindowBoundsInScreen(
          dragged_window, SplitViewController::RIGHT);
    case IndicatorState::kPreviewAreaRight:
      return split_view_controller->GetSnappedWindowBoundsInScreen(
          dragged_window, SplitViewController::LEFT);
    default:
      return screen_util::
          GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
              dragged_window);
  }
}

// Returns the desks widget bounds in root, given the screen bounds of the
// overview grid.
gfx::Rect GetDesksWidgetBounds(aura::Window* root,
                               const gfx::Rect& overview_grid_screen_bounds) {
  gfx::Rect desks_widget_root_bounds = overview_grid_screen_bounds;
  ::wm::ConvertRectFromScreen(root, &desks_widget_root_bounds);
  desks_widget_root_bounds.set_height(DesksBarView::GetBarHeight());
  return screen_util::SnapBoundsToDisplayEdge(desks_widget_root_bounds, root);
}

}  // namespace

// The class to observe the overview window that the dragged tabs will merge
// into. After the dragged tabs merge into the overview window, and if the
// overview window represents a minimized window, we need to update the
// overview minimized widget's content view so that it reflects the merge.
class OverviewGrid::TargetWindowObserver : public aura::WindowObserver {
 public:
  TargetWindowObserver() = default;
  ~TargetWindowObserver() override { StopObserving(); }

  void StartObserving(aura::Window* window) {
    if (target_window_)
      StopObserving();

    target_window_ = window;
    target_window_->AddObserver(this);
  }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    DCHECK_EQ(window, target_window_);
    // When the property is cleared, the dragged window should have been merged
    // into |target_window_|, update the corresponding window item in overview.
    if (key == ash::kIsDeferredTabDraggingTargetWindowKey &&
        !window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey)) {
      UpdateWindowItemInOverviewContaining(window);
      StopObserving();
    }
  }

  void OnWindowDestroying(aura::Window* window) override {
    DCHECK_EQ(window, target_window_);
    StopObserving();
  }

 private:
  void UpdateWindowItemInOverviewContaining(aura::Window* window) {
    OverviewController* overview_controller =
        Shell::Get()->overview_controller();
    if (!overview_controller->InOverviewSession())
      return;

    OverviewGrid* grid =
        overview_controller->overview_session()->GetGridWithRootWindow(
            window->GetRootWindow());
    if (!grid)
      return;

    OverviewItem* item = grid->GetOverviewItemContaining(window);
    if (!item)
      return;

    item->UpdateItemContentViewForMinimizedWindow();
  }

  void StopObserving() {
    if (target_window_)
      target_window_->RemoveObserver(this);
    target_window_ = nullptr;
  }

  aura::Window* target_window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TargetWindowObserver);
};

OverviewGrid::OverviewGrid(aura::Window* root_window,
                           const std::vector<aura::Window*>& windows,
                           OverviewSession* overview_session,
                           const gfx::Rect& bounds_in_screen)
    : root_window_(root_window),
      overview_session_(overview_session),
      window_observer_(this),
      window_state_observer_(this),
      bounds_(bounds_in_screen) {
  for (auto* window : windows) {
    if (window->GetRootWindow() != root_window)
      continue;

    // Hide the drag shadow if it is visible.
    Shell::Get()->resize_shadow_controller()->HideShadow(window);

    // Stop ongoing animations before entering overview mode. Because we are
    // deferring SetTransform of the windows beneath the window covering the
    // available workspace, we need to set the correct transforms of these
    // windows before entering overview mode again in the
    // OnImplicitAnimationsCompleted() of the observer of the
    // available-workspace-covering window's animation.
    auto* animator = window->layer()->GetAnimator();
    if (animator->is_animating())
      window->layer()->GetAnimator()->StopAnimating();
    window_observer_.Add(window);
    window_state_observer_.Add(wm::GetWindowState(window));
    window_list_.push_back(
        std::make_unique<OverviewItem>(window, overview_session_, this));
  }
}

OverviewGrid::~OverviewGrid() = default;

void OverviewGrid::Shutdown() {
  ScreenRotationAnimator::GetForRootWindow(root_window_)->RemoveObserver(this);

  bool has_non_cover_animating = false;
  int animate_count = 0;

  for (const auto& window : window_list_) {
    if (window->should_animate_when_exiting() && !has_non_cover_animating) {
      has_non_cover_animating |=
          !CanCoverAvailableWorkspace(window->GetWindow());
      animate_count++;
    }
    window->Shutdown();
  }
  bool single_animation_in_clamshell =
      (animate_count == 1 && !has_non_cover_animating) &&
      !Shell::Get()->tablet_mode_controller()->InTabletMode();

  // OverviewGrid in splitscreen does not include the window to be activated.
  if (!window_list_.empty() ||
      Shell::Get()->split_view_controller()->InSplitViewMode()) {
    // The following instance self-destructs when shutdown animation ends.
    new ShutdownAnimationFpsCounterObserver(
        root_window_->layer()->GetCompositor(), single_animation_in_clamshell);
  }

  overview_session_ = nullptr;

  while (!window_list_.empty())
    RemoveItem(window_list_.back().get());
}

void OverviewGrid::PrepareForOverview() {
  if (!ShouldAnimateWallpaper())
    MaybeInitDesksWidget();

  for (const auto& window : window_list_)
    window->PrepareForOverview();
  prepared_for_overview_ = true;
  if (Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    ScreenRotationAnimator::GetForRootWindow(root_window_)->AddObserver(this);
  }
}

void OverviewGrid::PositionWindows(
    bool animate,
    const base::flat_set<OverviewItem*>& ignored_items,
    OverviewSession::OverviewTransition transition) {
  if (!overview_session_ || suspend_reposition_ || window_list_.empty())
    return;

  DCHECK_NE(transition, OverviewSession::OverviewTransition::kExit);

  std::vector<gfx::RectF> rects = GetWindowRects(ignored_items);
  if (transition == OverviewSession::OverviewTransition::kEnter) {
    CalculateWindowListAnimationStates(/*selected_item=*/nullptr, transition,
                                       rects);
  }

  // Position the windows centering the left-aligned rows vertically. Do not
  // position items in |ignored_items|.
  OverviewAnimationType animation_type =
      transition == OverviewSession::OverviewTransition::kEnter
          ? OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER
          : OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW;

  int animate_count = 0;
  bool has_non_cover_animating = false;
  std::vector<OverviewAnimationType> animation_types(rects.size());

  for (size_t i = 0; i < window_list_.size(); ++i) {
    OverviewItem* window_item = window_list_[i].get();
    if (window_item->animating_to_close() ||
        ignored_items.contains(window_item)) {
      rects[i].SetRect(0, 0, 0, 0);
      continue;
    }

    // Calculate if each window item needs animation.
    bool should_animate_item = animate;
    // If we're in entering overview process, not all window items in the grid
    // might need animation even if the grid needs animation.
    if (animate && transition == OverviewSession::OverviewTransition::kEnter)
      should_animate_item = window_item->should_animate_when_entering();
    // Do not do the bounds animation for the drop target. We'll do the opacity
    // animation by ourselves.
    if (IsDropTargetWindow(window_item->GetWindow()))
      should_animate_item = false;
    if (animate && transition == OverviewSession::OverviewTransition::kEnter) {
      if (window_item->should_animate_when_entering() &&
          !has_non_cover_animating) {
        has_non_cover_animating |=
            !CanCoverAvailableWorkspace(window_item->GetWindow());
        animate_count++;
      }
    }
    animation_types[i] =
        should_animate_item ? animation_type : OVERVIEW_ANIMATION_NONE;
  }

  if (animate && transition == OverviewSession::OverviewTransition::kEnter &&
      !window_list_.empty()) {
    bool single_animation_in_clamshell =
        animate_count == 1 && !has_non_cover_animating &&
        !Shell::Get()->tablet_mode_controller()->InTabletMode();
    fps_counter_ = std::make_unique<OverviewEnterFpsCounter>(
        window_list_[0]->GetWindow()->layer()->GetCompositor(),
        single_animation_in_clamshell);
  }

  // Apply the animation after creating fps_counter_ so that unit test
  // can correctly count the measure requests.
  for (size_t i = 0; i < window_list_.size(); ++i) {
    if (rects[i].IsEmpty())
      continue;
    OverviewItem* window_item = window_list_[i].get();
    window_item->SetBounds(rects[i], animation_types[i]);
  }

  // If the selection widget is active, reposition it without any animation.
  if (selection_widget_)
    MoveSelectionWidgetToTarget(animate);
}

bool OverviewGrid::Move(OverviewSession::Direction direction, bool animate) {
  if (empty())
    return true;

  bool recreate_selection_widget = false;
  bool out_of_bounds = false;
  bool changed_selection_index = false;
  gfx::RectF old_bounds;
  if (SelectedWindow()) {
    old_bounds = SelectedWindow()->target_bounds();
    // Make the old selected window header non-transparent first.
    SelectedWindow()->set_selected(false);
  }

  // TODO(dantonvu|sammiequon): Refactor this so OverviewSession controls and
  // owns |selection_widget_|. Also rename selection related names to
  // highlighted as selection can also mean something else in overview.

  // [up] key is equivalent to [left] key and [down] key is equivalent to
  // [right] key.
  if (!selection_widget_) {
    switch (direction) {
      case OverviewSession::UP:
      case OverviewSession::LEFT:
        selected_index_ = window_list_.size() - 1;
        break;
      case OverviewSession::DOWN:
      case OverviewSession::RIGHT:
        selected_index_ = 0;
        break;
    }
    changed_selection_index = true;
  }

  // Checks number of windows present during a session. If there's only one
  // window being used across all displays, allow the user to highlight the
  // window but unable to move it.
  // TODO(sammiequon): Investigate if we can remove this call.
  if (overview_session()->NumWindowsTotal() == 1u) {
    MoveSelectionWidget(direction, recreate_selection_widget, out_of_bounds,
                        animate);
    return true;
  }

  while (!changed_selection_index) {
    switch (direction) {
      case OverviewSession::UP:
      case OverviewSession::LEFT:
        if (selected_index_ == 0)
          out_of_bounds = true;
        --selected_index_;
        break;
      case OverviewSession::DOWN:
      case OverviewSession::RIGHT:
        if (selected_index_ >= window_list_.size() - 1)
          out_of_bounds = true;
        ++selected_index_;
        break;
    }
    if (!out_of_bounds && SelectedWindow()) {
      if (SelectedWindow()->target_bounds().y() != old_bounds.y())
        recreate_selection_widget = true;
    }
    changed_selection_index = true;
  }
  MoveSelectionWidget(direction, recreate_selection_widget, out_of_bounds,
                      animate);

  // Make the new selected window header fully transparent.
  if (SelectedWindow())
    SelectedWindow()->set_selected(true);
  return out_of_bounds;
}

OverviewItem* OverviewGrid::SelectedWindow() const {
  if (!selection_widget_)
    return nullptr;
  CHECK(selected_index_ < window_list_.size());
  return window_list_[selected_index_].get();
}

OverviewItem* OverviewGrid::GetOverviewItemContaining(
    const aura::Window* window) const {
  for (const auto& window_item : window_list_) {
    if (window_item && window_item->Contains(window))
      return window_item.get();
  }
  return nullptr;
}

void OverviewGrid::AddItem(aura::Window* window,
                           bool reposition,
                           bool animate,
                           const base::flat_set<OverviewItem*>& ignored_items,
                           size_t index) {
  DCHECK(!GetOverviewItemContaining(window));
  DCHECK_LE(index, window_list_.size());

  window_observer_.Add(window);
  window_state_observer_.Add(wm::GetWindowState(window));
  window_list_.insert(
      window_list_.begin() + index,
      std::make_unique<OverviewItem>(window, overview_session_, this));
  window_list_[index]->PrepareForOverview();
  // The item is added after overview enter animation is complete, so
  // just call OnStartingAnimationComplete.
  window_list_[index]->OnStartingAnimationComplete();

  if (reposition)
    PositionWindows(animate, ignored_items);
}

void OverviewGrid::AppendItem(aura::Window* window,
                              bool reposition,
                              bool animate) {
  AddItem(window, reposition, animate, /*ignored_items=*/{},
          window_list_.size());
}

void OverviewGrid::RemoveItem(OverviewItem* overview_item) {
  auto* window = overview_item->GetWindow();
  // Use reverse iterator to be efficiently when removing all.
  auto iter = std::find_if(window_list_.rbegin(), window_list_.rend(),
                           [window](std::unique_ptr<OverviewItem>& item) {
                             return item->GetWindow() == window;
                           });
  DCHECK(iter != window_list_.rend());
  window_observer_.Remove(window);
  window_state_observer_.Remove(wm::GetWindowState(window));
  // Erase from the list first because deleting OverviewItem can lead to
  // iterating through the |window_list_|.
  std::unique_ptr<OverviewItem> tmp = std::move(*iter);
  window_list_.erase(std::next(iter).base());
}

void OverviewGrid::AddDropTargetForDraggingFromOverview(
    OverviewItem* dragged_item) {
  DCHECK_EQ(dragged_item->GetWindow()->GetRootWindow(), root_window_);
  DCHECK(!drop_target_widget_);
  drop_target_widget_ =
      CreateDropTargetWidget(dragged_item->GetWindow(), /*animate=*/false);
  const size_t position = GetOverviewItemIndex(dragged_item) + 1u;
  overview_session_->AddItem(drop_target_widget_->GetNativeWindow(),
                             /*reposition=*/true, /*animate=*/false,
                             /*ignored_items=*/{dragged_item}, position);
  if (selection_widget_ && selected_index_ >= position)
    ++selected_index_;
  // This part is necessary because |OverviewItem::OnSelectorItemDragStarted| is
  // called on all overview items before the drop target exists among them. That
  // is because |AddDropTargetForDraggingFromOverview| is only called for drag
  // to snap, but |OnSelectorItemDragStarted| is called before the drag has been
  // disambiguated between drag to close and drag to snap.
  GetDropTarget()->OnSelectorItemDragStarted(dragged_item);
}

void OverviewGrid::RemoveDropTarget() {
  DCHECK(drop_target_widget_);
  OverviewItem* drop_target = GetDropTarget();
  if (selection_widget_ && selected_index_ >= GetOverviewItemIndex(drop_target))
    --selected_index_;
  overview_session_->RemoveItem(drop_target);
  drop_target_widget_.reset();
}

void OverviewGrid::SetBoundsAndUpdatePositions(
    const gfx::Rect& bounds_in_screen,
    const base::flat_set<OverviewItem*>& ignored_items) {
  bounds_ = bounds_in_screen;
  if (desks_widget_)
    desks_widget_->SetBounds(GetDesksWidgetBounds(root_window_, bounds_));
  PositionWindows(/*animate=*/true, ignored_items);
}

void OverviewGrid::RearrangeDuringDrag(aura::Window* dragged_window,
                                       IndicatorState indicator_state) {
  OverviewItem* drop_target = GetDropTarget();

  // Update the drop target visibility according to |indicator_state|.
  const bool wanted_drop_target_visibility =
      indicator_state != IndicatorState::kPreviewAreaLeft &&
      indicator_state != IndicatorState::kPreviewAreaRight;
  const bool update_drop_target_visibility =
      drop_target &&
      (drop_target_widget_->IsVisible() != wanted_drop_target_visibility);
  if (update_drop_target_visibility) {
    drop_target_widget_->GetLayer()->SetVisible(wanted_drop_target_visibility);
    drop_target->SetOpacity(wanted_drop_target_visibility ? 1.f : 0.f);
  }

  // Update the grid's bounds.
  const gfx::Rect wanted_grid_bounds =
      GetGridBoundsInScreenDuringDragging(dragged_window, indicator_state);
  if (update_drop_target_visibility || bounds_ != wanted_grid_bounds) {
    OverviewItem* dragged_item = GetOverviewItemContaining(dragged_window);
    base::flat_set<OverviewItem*> ignored_items;
    if (dragged_item)
      ignored_items.insert(dragged_item);
    if (drop_target && !wanted_drop_target_visibility)
      ignored_items.insert(drop_target);
    SetBoundsAndUpdatePositions(wanted_grid_bounds, ignored_items);
  }
}

void OverviewGrid::UpdateDropTargetBackgroundVisibility(
    OverviewItem* dragged_item,
    const gfx::PointF& location_in_screen) {
  DCHECK(drop_target_widget_);
  aura::Window* target_window =
      GetTargetWindowOnLocation(location_in_screen, dragged_item);
  DropTargetView* drop_target_view =
      static_cast<DropTargetView*>(drop_target_widget_->GetContentsView());
  DCHECK(drop_target_view);
  drop_target_view->UpdateBackgroundVisibility(
      target_window && IsDropTargetWindow(target_window));
}

void OverviewGrid::SetSelectionWidgetVisibility(bool visible) {
  if (!selection_widget_)
    return;

  if (visible)
    selection_widget_->Show();
  else
    selection_widget_->Hide();
}

void OverviewGrid::UpdateCannotSnapWarningVisibility() {
  for (auto& overview_mode_item : window_list_)
    overview_mode_item->UpdateCannotSnapWarningVisibility();
}

void OverviewGrid::OnSelectorItemDragStarted(OverviewItem* item) {
  for (auto& overview_mode_item : window_list_)
    overview_mode_item->OnSelectorItemDragStarted(item);
}

void OverviewGrid::OnSelectorItemDragEnded(bool snap) {
  for (auto& overview_mode_item : window_list_)
    overview_mode_item->OnSelectorItemDragEnded(snap);
}

void OverviewGrid::OnWindowDragStarted(aura::Window* dragged_window,
                                       bool animate) {
  DCHECK_EQ(dragged_window->GetRootWindow(), root_window_);
  DCHECK(!drop_target_widget_);
  drop_target_widget_ = CreateDropTargetWidget(dragged_window, animate);
  overview_session_->AddItem(drop_target_widget_->GetNativeWindow(),
                             /*reposition=*/true, animate);
  if (selection_widget_)
    ++selected_index_;

  // Stack the |dragged_window| at top during drag.
  dragged_window->parent()->StackChildAtTop(dragged_window);

  // Called to set caption and title visibility during dragging.
  OnSelectorItemDragStarted(/*item=*/nullptr);
}

void OverviewGrid::OnWindowDragContinued(aura::Window* dragged_window,
                                         const gfx::PointF& location_in_screen,
                                         IndicatorState indicator_state) {
  DCHECK_EQ(dragged_window->GetRootWindow(), root_window_);

  RearrangeDuringDrag(dragged_window, indicator_state);
  UpdateDropTargetBackgroundVisibility(nullptr, location_in_screen);

  aura::Window* target_window =
      GetTargetWindowOnLocation(location_in_screen, /*ignored_item=*/nullptr);

  if (indicator_state == IndicatorState::kPreviewAreaLeft ||
      indicator_state == IndicatorState::kPreviewAreaRight) {
    // If the dragged window is currently dragged into preview window area,
    // clear the selection widget.
    if (SelectedWindow()) {
      SelectedWindow()->set_selected(false);
      selection_widget_.reset();
    }

    // Also clear ash::kIsDeferredTabDraggingTargetWindowKey key on the target
    // overview item so that it can't merge into this overview item if the
    // dragged window is currently in preview window area.
    if (target_window && !IsDropTargetWindow(target_window))
      target_window->ClearProperty(ash::kIsDeferredTabDraggingTargetWindowKey);

    return;
  }

  // Show the selection widget if |location_in_screen| is contained by the
  // browser windows' overview item in overview.
  if (target_window &&
      target_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey)) {
    size_t previous_selected_index = selected_index_;
    selected_index_ = GetOverviewItemIterContainingWindow(target_window) -
                      window_list_.begin();
    if (previous_selected_index == selected_index_ && selection_widget_)
      return;

    if (previous_selected_index != selected_index_)
      selection_widget_.reset();

    const OverviewSession::Direction direction =
        (selected_index_ - previous_selected_index > 0) ? OverviewSession::RIGHT
                                                        : OverviewSession::LEFT;
    MoveSelectionWidget(direction,
                        /*recreate_selection_widget=*/true,
                        /*out_of_bounds=*/false,
                        /*animate=*/false);
    return;
  }

  if (SelectedWindow()) {
    SelectedWindow()->set_selected(false);
    selection_widget_.reset();
  }
}

void OverviewGrid::OnWindowDragEnded(aura::Window* dragged_window,
                                     const gfx::PointF& location_in_screen,
                                     bool should_drop_window_into_overview,
                                     bool snap) {
  DCHECK_EQ(dragged_window->GetRootWindow(), root_window_);
  DCHECK(drop_target_widget_.get());

  // Add the dragged window into drop target in overview if
  // |should_drop_window_into_overview| is true. Only consider add the dragged
  // window into drop target if SelectedWindow is false since drop target will
  // not be selected and tab dragging might drag a tab window to merge it into a
  // browser window in overview.
  if (SelectedWindow()) {
    SelectedWindow()->set_selected(false);
    selection_widget_.reset();
  } else if (should_drop_window_into_overview) {
    AddDraggedWindowIntoOverviewOnDragEnd(dragged_window);
  }

  RemoveDropTarget();

  // Called to reset caption and title visibility after dragging.
  OnSelectorItemDragEnded(snap);

  // After drag ends, if the dragged window needs to merge into another window
  // |target_window|, and we may need to update |minimized_widget_| that holds
  // the contents of |target_window| if |target_window| is a minimized window
  // in overview.
  aura::Window* target_window =
      GetTargetWindowOnLocation(location_in_screen, /*ignored_item=*/nullptr);
  if (target_window &&
      target_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey)) {
    // Create an window observer and update the minimized window widget after
    // the dragged window merges into |target_window|.
    if (!target_window_observer_)
      target_window_observer_ = std::make_unique<TargetWindowObserver>();
    target_window_observer_->StartObserving(target_window);
  }

  // Update the grid bounds and reposition windows. Since the grid bounds might
  // be updated based on the preview area during drag, but the window finally
  // didn't be snapped to the preview area.
  SetBoundsAndUpdatePositions(
      GetGridBoundsInScreenAfterDragging(dragged_window), /*ignored_items=*/{});
}

bool OverviewGrid::IsDropTargetWindow(aura::Window* window) const {
  return drop_target_widget_ &&
         drop_target_widget_->GetNativeWindow() == window;
}

OverviewItem* OverviewGrid::GetDropTarget() {
  return drop_target_widget_
             ? GetOverviewItemContaining(drop_target_widget_->GetNativeWindow())
             : nullptr;
}

void OverviewGrid::OnWindowDestroying(aura::Window* window) {
  window_observer_.Remove(window);
  window_state_observer_.Remove(wm::GetWindowState(window));
  auto iter = GetOverviewItemIterContainingWindow(window);
  DCHECK(iter != window_list_.end());

  // Windows that are animating to a close state already call PositionWindows,
  // no need to call it twice.
  const bool needs_repositioning = !((*iter)->animating_to_close());

  size_t removed_index = iter - window_list_.begin();
  // Erase from the list first because deleting OverviewItem can lead to
  // iterating through the |window_list_|.
  std::unique_ptr<OverviewItem> tmp = std::move(*iter);
  window_list_.erase(iter);
  tmp.reset();

  if (empty()) {
    selection_widget_.reset();
    // If the grid is now empty, notify |overview_session_| so that it erases us
    // from its grid list.
    if (overview_session_)
      overview_session_->OnGridEmpty(this);
    return;
  }

  // If selecting, update the selection index.
  if (selection_widget_) {
    bool send_focus_alert = selected_index_ == removed_index;
    if (selected_index_ >= removed_index && selected_index_ != 0)
      selected_index_--;
    SelectedWindow()->set_selected(true);
    if (send_focus_alert)
      SelectedWindow()->SendAccessibleSelectionEvent();
  }

  if (needs_repositioning)
    PositionWindows(true);
}

void OverviewGrid::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds,
                                         ui::PropertyChangeReason reason) {
  // During preparation, window bounds can change. Ignore bounds
  // change notifications in this case; we'll reposition soon.
  if (!prepared_for_overview_)
    return;

  // |drop_target_widget_| will get its bounds set as opposed to its transform
  // set in |OverviewItem::SetItemBounds| so do not position windows again when
  // that particular window has its bounds changed.
  if (IsDropTargetWindow(window))
    return;

  auto iter = GetOverviewItemIterContainingWindow(window);
  DCHECK(iter != window_list_.end());

  // Immediately finish any active bounds animation.
  window->layer()->GetAnimator()->StopAnimatingProperty(
      ui::LayerAnimationElement::BOUNDS);
  (*iter)->UpdateWindowDimensionsType();
  PositionWindows(false);
}

void OverviewGrid::OnWindowPropertyChanged(aura::Window* window,
                                           const void* key,
                                           intptr_t old) {
  if (prepared_for_overview_ && key == aura::client::kTopViewInset &&
      window->GetProperty(aura::client::kTopViewInset) !=
          static_cast<int>(old)) {
    PositionWindows(/*animate=*/false);
  }
}

void OverviewGrid::OnPostWindowStateTypeChange(wm::WindowState* window_state,
                                               WindowStateType old_type) {
  // During preparation, window state can change, e.g. updating shelf
  // visibility may show the temporarily hidden (minimized) panels.
  if (!prepared_for_overview_)
    return;

  // When swiping away overview mode via shelf, windows will get minimized, but
  // we do not want to create minimized widgets in their place.
  if (overview_session_->enter_exit_overview_type() ==
      OverviewSession::EnterExitOverviewType::kSwipeFromShelf) {
    return;
  }

  WindowStateType new_type = window_state->GetStateType();
  if (IsMinimizedWindowStateType(old_type) ==
      IsMinimizedWindowStateType(new_type)) {
    return;
  }

  auto iter = std::find_if(window_list_.begin(), window_list_.end(),
                           [window_state](std::unique_ptr<OverviewItem>& item) {
                             return item->Contains(window_state->window());
                           });
  if (iter != window_list_.end()) {
    (*iter)->OnMinimizedStateChanged();
    PositionWindows(/*animate=*/false);
  }
}

void OverviewGrid::OnScreenCopiedBeforeRotation() {
  for (auto& window : window_list()) {
    window->set_disable_mask(true);
    window->UpdateMaskAndShadow();
  }
}

void OverviewGrid::OnScreenRotationAnimationFinished(
    ScreenRotationAnimator* animator,
    bool canceled) {
  for (auto& window : window_list())
    window->set_disable_mask(false);
  Shell::Get()->overview_controller()->DelayedUpdateMaskAndShadow();
}

void OverviewGrid::OnStartingAnimationComplete(bool canceled) {
  fps_counter_.reset();
  if (canceled)
    return;

  MaybeInitDesksWidget();

  for (auto& window : window_list())
    window->OnStartingAnimationComplete();
}

bool OverviewGrid::ShouldAnimateWallpaper() const {
  // Kiosk next shell mode will have an opaque background covering the wallpaper
  // prior to entering overview, so there's no need to animate it.
  if (Shell::Get()->kiosk_next_shell_controller()->IsEnabled())
    return false;

  // Never animate when doing app dragging or when immediately exiting.
  const auto enter_exit_type = overview_session_->enter_exit_overview_type();
  if (enter_exit_type ==
          OverviewSession::EnterExitOverviewType::kWindowDragged ||
      enter_exit_type ==
          OverviewSession::EnterExitOverviewType::kImmediateExit) {
    return false;
  }

  // If one of the windows covers the workspace, we do not need to animate.
  for (const auto& overview_item : window_list_) {
    if (CanCoverAvailableWorkspace(overview_item->GetWindow()))
      return false;
  }

  return true;
}

void OverviewGrid::CalculateWindowListAnimationStates(
    OverviewItem* selected_item,
    OverviewSession::OverviewTransition transition,
    const std::vector<gfx::RectF>& target_bounds) {
  using OverviewTransition = OverviewSession::OverviewTransition;

  // Sanity checks to enforce assumptions used in later codes.
  switch (transition) {
    case OverviewTransition::kEnter:
      DCHECK_EQ(target_bounds.size(), window_list_.size());
      break;
    case OverviewTransition::kExit:
      DCHECK(target_bounds.empty());
      break;
    default:
      NOTREACHED();
  }

  // Create a copy of |window_list_| which has always on top windows in the
  // front.
  std::vector<OverviewItem*> items;
  std::transform(
      window_list_.begin(), window_list_.end(), std::back_inserter(items),
      [](const std::unique_ptr<OverviewItem>& item) -> OverviewItem* {
        return item.get();
      });
  // Sort items by:
  // 1) Selected items that are always on top windows.
  // 2) Other always on top windows.
  // 3) Selected items that are not always on top windows.
  // 4) Other not always on top windows.
  // Preserves ordering if the category is the same.
  std::sort(items.begin(), items.end(),
            [&selected_item](OverviewItem* a, OverviewItem* b) {
              const bool a_on_top =
                  a->GetWindow()->GetProperty(aura::client::kAlwaysOnTopKey);
              const bool b_on_top =
                  b->GetWindow()->GetProperty(aura::client::kAlwaysOnTopKey);
              if (selected_item && a_on_top && b_on_top)
                return a == selected_item;
              if (a_on_top)
                return true;
              if (b_on_top)
                return false;
              if (selected_item)
                return a == selected_item;
              return false;
            });

  SkRegion occluded_region;
  for (size_t i = 0; i < items.size(); ++i) {
    const bool minimized =
        wm::GetWindowState(items[i]->GetWindow())->IsMinimized();
    bool src_occluded = minimized;
    bool dst_occluded = false;
    gfx::Rect src_bounds_temp =
        minimized ? gfx::Rect()
                  : items[i]->GetWindow()->GetBoundsInRootWindow();
    if (!src_bounds_temp.IsEmpty()) {
      if (transition == OverviewTransition::kEnter &&
          Shell::Get()->tablet_mode_controller()->InTabletMode()) {
        BackdropController* backdrop_controller =
            GetActiveWorkspaceController(root_window_)
                ->layout_manager()
                ->backdrop_controller();
        if (backdrop_controller->GetTopmostWindowWithBackdrop() ==
            items[i]->GetWindow()) {
          src_bounds_temp = screen_util::GetDisplayWorkAreaBoundsInParent(
              items[i]->GetWindow());
        }
      } else if (transition == OverviewTransition::kExit) {
        // On exiting overview, |GetBoundsInRootWindow()| will have the overview
        // translation applied to it, so use |bounds()| and
        // |ConvertRectToScreen()| to get the true target bounds.
        src_bounds_temp = items[i]->GetWindow()->bounds();
        ::wm::ConvertRectToScreen(items[i]->root_window(), &src_bounds_temp);
      }
    }
    SkIRect src_bounds = gfx::RectToSkIRect(src_bounds_temp);
    SkIRect dst_bounds = gfx::RectToSkIRect(gfx::ToEnclosedRect(
        transition == OverviewTransition::kEnter ? target_bounds[i]
                                                 : items[i]->target_bounds()));

    if (!occluded_region.isEmpty()) {
      src_occluded |=
          (!src_bounds.isEmpty() && occluded_region.contains(src_bounds));
      dst_occluded |= occluded_region.contains(dst_bounds);
    }

    // Add |src_bounds| to our region if it is not empty (minimized window).
    if (!src_bounds.isEmpty())
      occluded_region.op(src_bounds, SkRegion::kUnion_Op);

    const bool should_animate = !(src_occluded && dst_occluded);
    if (transition == OverviewSession::OverviewTransition::kEnter)
      items[i]->set_should_animate_when_entering(should_animate);
    else if (transition == OverviewSession::OverviewTransition::kExit)
      items[i]->set_should_animate_when_exiting(should_animate);
  }
}

void OverviewGrid::SetWindowListNotAnimatedWhenExiting() {
  should_animate_when_exiting_ = false;
  for (const auto& item : window_list_)
    item->set_should_animate_when_exiting(false);
}

void OverviewGrid::StartNudge(OverviewItem* item) {
  // When there is one window left, there is no need to nudge.
  if (window_list_.size() <= 1) {
    nudge_data_.clear();
    return;
  }

  // If any of the items are being animated to close, do not nudge any windows
  // otherwise we have to deal with potential items getting removed from
  // |window_list_| midway through a nudge.
  for (const auto& window_item : window_list_) {
    if (window_item->animating_to_close()) {
      nudge_data_.clear();
      return;
    }
  }

  DCHECK(item);

  // Get the bounds of the windows currently, and the bounds if |item| were to
  // be removed.
  std::vector<gfx::RectF> src_rects;
  for (const auto& window_item : window_list_)
    src_rects.push_back(window_item->target_bounds());

  std::vector<gfx::RectF> dst_rects = GetWindowRects({item});

  const size_t index = GetOverviewItemIndex(item);

  // Returns a vector of integers indicating which row the item is in. |index|
  // is the index of the element which is going to be deleted and should not
  // factor into calculations. The call site should mark |index| as -1 if it
  // should not be used. The item at |index| is marked with a 0. The heights of
  // items are all set to the same value so a new row is determined if the y
  // value has changed from the previous item.
  auto get_rows = [](const std::vector<gfx::RectF>& bounds_list, size_t index) {
    std::vector<int> row_numbers;
    int current_row = 1;
    float last_y = 0;
    for (size_t i = 0; i < bounds_list.size(); ++i) {
      if (i == index) {
        row_numbers.push_back(0);
        continue;
      }

      // Update |current_row| if the y position has changed (heights are all
      // equal in overview, so a new y position indicates a new row).
      if (last_y != 0 && last_y != bounds_list[i].y())
        ++current_row;

      row_numbers.push_back(current_row);
      last_y = bounds_list[i].y();
    }

    return row_numbers;
  };

  std::vector<int> src_rows = get_rows(src_rects, -1);
  std::vector<int> dst_rows = get_rows(dst_rects, index);

  // Do nothing if the number of rows change.
  if (dst_rows.back() != 0 && src_rows.back() != dst_rows.back())
    return;
  size_t second_last_index = src_rows.size() - 2;
  if (dst_rows.back() == 0 &&
      src_rows[second_last_index] != dst_rows[second_last_index]) {
    return;
  }

  // Do nothing if the last item from the previous row will drop onto the
  // current row, this will cause the items in the current row to shift to the
  // right while the previous item stays in the previous row, which looks weird.
  if (src_rows[index] > 1) {
    // Find the last item from the previous row.
    size_t previous_row_last_index = index;
    while (src_rows[previous_row_last_index] == src_rows[index]) {
      --previous_row_last_index;
    }

    // Early return if the last item in the previous row changes rows.
    if (src_rows[previous_row_last_index] != dst_rows[previous_row_last_index])
      return;
  }

  // Helper to check whether the item at |item_index| will be nudged.
  auto should_nudge = [&src_rows, &dst_rows, &index](size_t item_index) {
    // Out of bounds.
    if (item_index >= src_rows.size())
      return false;

    // Nudging happens when the item stays on the same row and is also on the
    // same row as the item to be deleted was.
    if (dst_rows[item_index] == src_rows[index] &&
        dst_rows[item_index] == src_rows[item_index]) {
      return true;
    }

    return false;
  };

  // Starting from |index| go up and down while the nudge condition returns
  // true.
  std::vector<int> affected_indexes;
  size_t loop_index;

  if (index > 0) {
    loop_index = index - 1;
    while (should_nudge(loop_index)) {
      affected_indexes.push_back(loop_index);
      --loop_index;
    }
  }

  loop_index = index + 1;
  while (should_nudge(loop_index)) {
    affected_indexes.push_back(loop_index);
    ++loop_index;
  }

  // Populate |nudge_data_| with the indexes in |affected_indexes| and their
  // respective source and destination bounds.
  nudge_data_.resize(affected_indexes.size());
  for (size_t i = 0; i < affected_indexes.size(); ++i) {
    NudgeData data;
    data.index = affected_indexes[i];
    data.src = src_rects[data.index];
    data.dst = dst_rects[data.index];
    nudge_data_[i] = data;
  }
}

void OverviewGrid::UpdateNudge(OverviewItem* item, double value) {
  for (const auto& data : nudge_data_) {
    DCHECK_LT(data.index, window_list_.size());

    OverviewItem* nudged_item = window_list_[data.index].get();
    double nudge_param = value * value / 30.0;
    nudge_param = base::ClampToRange(nudge_param, 0.0, 1.0);
    gfx::RectF bounds =
        gfx::Tween::RectFValueBetween(nudge_param, data.src, data.dst);
    nudged_item->SetBounds(bounds, OVERVIEW_ANIMATION_NONE);
  }
}

void OverviewGrid::EndNudge() {
  nudge_data_.clear();
}

void OverviewGrid::SlideWindowsIn() {
  for (const auto& window_item : window_list_)
    window_item->SlideWindowIn();
}

std::unique_ptr<ui::ScopedLayerAnimationSettings>
OverviewGrid::UpdateYPositionAndOpacity(
    int new_y,
    float opacity,
    OverviewSession::UpdateAnimationSettingsCallback callback) {
  DCHECK(!window_list_.empty());
  // Translate the window items to |new_y| with the opacity. Observe the
  // animation of the first window.
  std::unique_ptr<ui::ScopedLayerAnimationSettings> settings_to_observe;
  for (const auto& window_item : window_list_) {
    auto new_settings =
        window_item->UpdateYPositionAndOpacity(new_y, opacity, callback);
    if (!settings_to_observe && new_settings)
      settings_to_observe = std::move(new_settings);
  }
  return settings_to_observe;
}

aura::Window* OverviewGrid::GetTargetWindowOnLocation(
    const gfx::PointF& location_in_screen,
    OverviewItem* ignored_item) {
  for (std::unique_ptr<OverviewItem>& item : window_list_) {
    if (item.get() == ignored_item)
      continue;
    if (item->target_bounds().Contains(location_in_screen))
      return item->GetWindow();
  }
  return nullptr;
}

bool OverviewGrid::IsDesksBarViewActive() const {
  DCHECK(desks_util::ShouldDesksBarBeCreated());

  // The desk bar view is not active if there is only a single desk when
  // overview is started. Once there are more than one desk, it should stay
  // active even if the 2nd to last desk is deleted.
  return DesksController::Get()->desks().size() > 1 ||
         (desks_bar_view_ && !desks_bar_view_->mini_views().empty());
}

gfx::Rect OverviewGrid::GetGridEffectiveBounds() const {
  if (!desks_util::ShouldDesksBarBeCreated() || !IsDesksBarViewActive())
    return bounds_;

  gfx::Rect effective_bounds = bounds_;
  effective_bounds.Inset(0, DesksBarView::GetBarHeight(), 0, 0);
  return effective_bounds;
}

bool OverviewGrid::UpdateDesksBarDragDetails(const gfx::Point& screen_location,
                                             bool for_drop) {
  DCHECK(desks_util::ShouldDesksBarBeCreated());

  const bool dragged_item_over_bar =
      desks_widget_->GetWindowBoundsInScreen().Contains(screen_location);
  desks_bar_view_->SetDragDetails(screen_location,
                                  !for_drop && dragged_item_over_bar);
  return dragged_item_over_bar;
}

bool OverviewGrid::MaybeDropItemOnDeskMiniView(
    const gfx::Point& screen_location,
    OverviewItem* drag_item) {
  DCHECK(desks_util::ShouldDesksBarBeCreated());

  // End the drag for the DesksBarView.
  if (!UpdateDesksBarDragDetails(screen_location, /*for_drop=*/true))
    return false;

  auto* desks_controller = DesksController::Get();
  for (auto& mini_view : desks_bar_view_->mini_views()) {
    if (!mini_view->IsPointOnMiniView(screen_location))
      continue;

    aura::Window* const dragged_window = drag_item->GetWindow();
    Desk* const target_desk = mini_view->desk();
    if (target_desk == desks_controller->active_desk())
      return false;

    desks_controller->MoveWindowFromActiveDeskTo(dragged_window, target_desk);
    // Restore the dragged item window, so that its transform is reset to
    // identity.
    drag_item->RestoreWindow(/*reset_transform=*/true);

    // The item no longer needs to be in the overview grid.
    overview_session_->RemoveItem(drag_item);
    return true;
  }

  return false;
}

void OverviewGrid::MaybeInitDesksWidget() {
  if (!desks_util::ShouldDesksBarBeCreated() || desks_widget_)
    return;

  desks_widget_ = DesksBarView::CreateDesksWidget(
      root_window_, GetDesksWidgetBounds(root_window_, bounds_));
  desks_bar_view_ = new DesksBarView;

  // The following order of function calls is significant: SetContentsView()
  // must be called before DesksBarView:: Init(). This is needed because the
  // desks mini views need to access the widget to get the root window in order
  // to know how to layout themselves.
  desks_widget_->SetContentsView(desks_bar_view_);
  desks_bar_view_->Init();

  desks_widget_->Show();
}

void OverviewGrid::InitSelectionWidget(OverviewSession::Direction direction) {
  selection_widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.keep_on_top = false;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.layer_type = ui::LAYER_SOLID_COLOR;
  params.accept_events = false;
  selection_widget_->set_focus_on_creation(false);
  params.parent = root_window_->GetChildById(kShellWindowId_WallpaperContainer);
  selection_widget_->Init(params);
  aura::Window* widget_window = selection_widget_->GetNativeWindow();
  widget_window->SetProperty(kHideInDeskMiniViewKey, true);
  // Disable the "bounce in" animation when showing the window.
  ::wm::SetWindowVisibilityAnimationTransition(widget_window,
                                               ::wm::ANIMATE_NONE);
  widget_window->layer()->SetColor(kWindowSelectionColor);
  widget_window->layer()->SetRoundedCornerRadius(
      {kWindowSelectionRadius, kWindowSelectionRadius, kWindowSelectionRadius,
       kWindowSelectionRadius});
  // Set the opacity to 0 initial so we can fade it in.
  widget_window->layer()->SetOpacity(0.f);
  selection_widget_->Show();

  gfx::Rect target_bounds =
      gfx::ToEnclosedRect(SelectedWindow()->target_bounds());
  ::wm::ConvertRectFromScreen(root_window_, &target_bounds);
  gfx::Vector2d fade_out_direction =
      GetSlideVectorForFadeIn(direction, target_bounds);
  widget_window->SetBounds(target_bounds - fade_out_direction);
  widget_window->SetName("OverviewModeSelector");

  selector_shadow_ = std::make_unique<ui::Shadow>();
  selector_shadow_->Init(kWindowSelectionShadowElevation);
  selector_shadow_->layer()->SetVisible(true);
  selection_widget_->GetLayer()->SetMasksToBounds(false);
  selection_widget_->GetLayer()->Add(selector_shadow_->layer());
  selector_shadow_->SetContentBounds(gfx::Rect(target_bounds.size()));
}

void OverviewGrid::MoveSelectionWidget(OverviewSession::Direction direction,
                                       bool recreate_selection_widget,
                                       bool out_of_bounds,
                                       bool animate) {
  // If the selection widget is already active, fade it out in the selection
  // direction.
  if (selection_widget_ && (recreate_selection_widget || out_of_bounds)) {
    if (overview_session_->enter_exit_overview_type() ==
        OverviewSession::EnterExitOverviewType::kImmediateExit) {
      ImmediatelyCloseWidgetOnExit(std::move(selection_widget_));
    } else {
      // Animate the old selection widget and then destroy it.
      views::Widget* old_selection = selection_widget_.get();
      aura::Window* old_selection_window = old_selection->GetNativeWindow();
      gfx::Vector2d fade_out_direction =
          GetSlideVectorForFadeIn(direction, old_selection_window->bounds());

      ScopedOverviewAnimationSettings settings(
          OVERVIEW_ANIMATION_SELECTION_WINDOW, old_selection_window);
      // CleanupAnimationObserver will delete itself (and the widget) when the
      // motion animation is complete. Ownership over the observer is passed to
      // the overview_session_->delegate() which has longer lifetime so that
      // animations can continue even after the overview session is shut down.
      std::unique_ptr<CleanupAnimationObserver> observer(
          new CleanupAnimationObserver(std::move(selection_widget_)));
      settings.AddObserver(observer.get());
      overview_session_->delegate()->AddExitAnimationObserver(
          std::move(observer));
      old_selection->SetOpacity(0.f);
      old_selection_window->SetBounds(old_selection_window->bounds() +
                                      fade_out_direction);
      old_selection->Hide();
    }
  }
  if (out_of_bounds)
    return;

  if (!selection_widget_)
    InitSelectionWidget(direction);
  // Send an a11y alert so that if ChromeVox is enabled, the item label is
  // read.
  SelectedWindow()->SendAccessibleSelectionEvent();
  // The selection widget is moved to the newly selected item in the same
  // grid.
  MoveSelectionWidgetToTarget(animate);
}

void OverviewGrid::MoveSelectionWidgetToTarget(bool animate) {
  gfx::Rect bounds = gfx::ToEnclosingRect(SelectedWindow()->target_bounds());
  ::wm::ConvertRectFromScreen(root_window_, &bounds);
  if (animate) {
    ScopedOverviewAnimationSettings settings(
        OVERVIEW_ANIMATION_SELECTION_WINDOW,
        selection_widget_->GetNativeWindow());
    selection_widget_->SetBounds(bounds);
    selection_widget_->SetOpacity(1.f);

    if (selector_shadow_) {
      ScopedOverviewAnimationSettings settings(
          OVERVIEW_ANIMATION_SELECTION_WINDOW_SHADOW,
          selector_shadow_->shadow_layer()->GetAnimator());
      bounds.Inset(1, 1);
      selector_shadow_->SetContentBounds(
          gfx::Rect(gfx::Point(1, 1), bounds.size()));
    }
    return;
  }
  selection_widget_->SetBounds(bounds);
  selection_widget_->SetOpacity(1.f);
  if (selector_shadow_) {
    bounds.Inset(1, 1);
    selector_shadow_->SetContentBounds(
        gfx::Rect(gfx::Point(1, 1), bounds.size()));
  }
}

std::vector<gfx::RectF> OverviewGrid::GetWindowRects(
    const base::flat_set<OverviewItem*>& ignored_items) {
  gfx::Rect total_bounds = GetGridEffectiveBounds();

  // Windows occupy vertically centered area with additional vertical insets.
  int horizontal_inset =
      gfx::ToFlooredInt(std::min(kOverviewInsetRatio * total_bounds.width(),
                                 kOverviewInsetRatio * total_bounds.height()));
  int vertical_inset =
      horizontal_inset +
      kOverviewVerticalInset * (total_bounds.height() - 2 * horizontal_inset);
  total_bounds.Inset(std::max(0, horizontal_inset - kWindowMargin),
                     std::max(0, vertical_inset - kWindowMargin));
  std::vector<gfx::RectF> rects;

  // Keep track of the lowest coordinate.
  int max_bottom = total_bounds.y();

  // Right bound of the narrowest row.
  int min_right = total_bounds.right();
  // Right bound of the widest row.
  int max_right = total_bounds.x();

  // Keep track of the difference between the narrowest and the widest row.
  // Initially this is set to the worst it can ever be assuming the windows fit.
  int width_diff = total_bounds.width();

  // Initially allow the windows to occupy all available width. Shrink this
  // available space horizontally to find the breakdown into rows that achieves
  // the minimal |width_diff|.
  int right_bound = total_bounds.right();

  // Determine the optimal height bisecting between |low_height| and
  // |high_height|. Once this optimal height is known, |height_fixed| is set to
  // true and the rows are balanced by repeatedly squeezing the widest row to
  // cause windows to overflow to the subsequent rows.
  int low_height = 2 * kWindowMargin;
  int high_height = std::max(low_height, total_bounds.height() + 1);
  int height = 0.5 * (low_height + high_height);
  bool height_fixed = false;

  // Repeatedly try to fit the windows |rects| within |right_bound|.
  // If a maximum |height| is found such that all window |rects| fit, this
  // fitting continues while shrinking the |right_bound| in order to balance the
  // rows. If the windows fit the |right_bound| would have been decremented at
  // least once so it needs to be incremented once before getting out of this
  // loop and one additional pass made to actually fit the |rects|.
  // If the |rects| cannot fit (e.g. there are too many windows) the bisection
  // will still finish and we might increment the |right_bound| once pixel extra
  // which is acceptable since there is an unused margin on the right.
  bool make_last_adjustment = false;
  while (true) {
    gfx::Rect overview_mode_bounds(total_bounds);
    overview_mode_bounds.set_width(right_bound - total_bounds.x());
    bool windows_fit = FitWindowRectsInBounds(
        overview_mode_bounds, std::min(kMaxHeight + 2 * kWindowMargin, height),
        ignored_items, &rects, &max_bottom, &min_right, &max_right);

    if (height_fixed) {
      if (!windows_fit) {
        // Revert the previous change to |right_bound| and do one last pass.
        right_bound++;
        make_last_adjustment = true;
        break;
      }
      // Break if all the windows are zero-width at the current scale.
      if (max_right <= total_bounds.x())
        break;
    } else {
      // Find the optimal row height bisecting between |low_height| and
      // |high_height|.
      if (windows_fit)
        low_height = height;
      else
        high_height = height;
      height = 0.5 * (low_height + high_height);
      // When height can no longer be improved, start balancing the rows.
      if (height == low_height)
        height_fixed = true;
    }

    if (windows_fit && height_fixed) {
      if (max_right - min_right <= width_diff) {
        // Row alignment is getting better. Try to shrink the |right_bound| in
        // order to squeeze the widest row.
        right_bound = max_right - 1;
        width_diff = max_right - min_right;
      } else {
        // Row alignment is getting worse.
        // Revert the previous change to |right_bound| and do one last pass.
        right_bound++;
        make_last_adjustment = true;
        break;
      }
    }
  }
  // Once the windows in |window_list_| no longer fit, the change to
  // |right_bound| was reverted. Perform one last pass to position the |rects|.
  if (make_last_adjustment) {
    gfx::Rect overview_mode_bounds(total_bounds);
    overview_mode_bounds.set_width(right_bound - total_bounds.x());
    FitWindowRectsInBounds(
        overview_mode_bounds, std::min(kMaxHeight + 2 * kWindowMargin, height),
        ignored_items, &rects, &max_bottom, &min_right, &max_right);
  }

  gfx::Vector2dF offset(0, (total_bounds.bottom() - max_bottom) / 2.f);
  for (size_t i = 0; i < rects.size(); ++i)
    rects[i] += offset;
  return rects;
}

bool OverviewGrid::FitWindowRectsInBounds(
    const gfx::Rect& bounds,
    int height,
    const base::flat_set<OverviewItem*>& ignored_items,
    std::vector<gfx::RectF>* out_rects,
    int* out_max_bottom,
    int* out_min_right,
    int* out_max_right) {
  const size_t window_count = window_list_.size();
  out_rects->resize(window_count);

  // Start in the top-left corner of |bounds|.
  int left = bounds.x();
  int top = bounds.y();

  // Keep track of the lowest coordinate.
  *out_max_bottom = bounds.y();

  // Right bound of the narrowest row.
  *out_min_right = bounds.right();
  // Right bound of the widest row.
  *out_max_right = bounds.x();

  // All elements are of same height and only the height is necessary to
  // determine each item's scale.
  const gfx::Size item_size(0, height);
  for (size_t i = 0u; i < window_count; ++i) {
    if (window_list_[i]->animating_to_close() ||
        ignored_items.contains(window_list_[i].get())) {
      continue;
    }

    const gfx::RectF target_bounds = window_list_[i]->GetTargetBoundsInScreen();
    int width = std::max(
        1, gfx::ToFlooredInt(target_bounds.width() *
                             window_list_[i]->GetItemScale(item_size)) +
               2 * kWindowMargin);
    switch (window_list_[i]->GetWindowDimensionsType()) {
      case ScopedOverviewTransformWindow::GridWindowFillMode::kLetterBoxed:
        width = ScopedOverviewTransformWindow::kExtremeWindowRatioThreshold *
                height;
        break;
      case ScopedOverviewTransformWindow::GridWindowFillMode::kPillarBoxed:
        width = height /
                ScopedOverviewTransformWindow::kExtremeWindowRatioThreshold;
        break;
      default:
        break;
    }

    if (left + width > bounds.right()) {
      // Move to the next row if possible.
      if (*out_min_right > left)
        *out_min_right = left;
      if (*out_max_right < left)
        *out_max_right = left;
      top += height;

      // Check if the new row reaches the bottom or if the first item in the new
      // row does not fit within the available width.
      if (top + height > bounds.bottom() ||
          bounds.x() + width > bounds.right()) {
        return false;
      }
      left = bounds.x();
    }

    // Position the current rect.
    (*out_rects)[i] = gfx::RectF(gfx::Rect(left, top, width, height));

    // Increment horizontal position using sanitized positive |width()|.
    left += gfx::ToRoundedInt((*out_rects)[i].width());

    *out_max_bottom = top + height;
  }

  // Update the narrowest and widest row width for the last row.
  if (*out_min_right > left)
    *out_min_right = left;
  if (*out_max_right < left)
    *out_max_right = left;

  return true;
}

std::vector<std::unique_ptr<OverviewItem>>::iterator
OverviewGrid::GetOverviewItemIterContainingWindow(aura::Window* window) {
  return std::find_if(window_list_.begin(), window_list_.end(),
                      [window](std::unique_ptr<OverviewItem>& item) {
                        return item->GetWindow() == window;
                      });
}

size_t OverviewGrid::GetOverviewItemIndex(OverviewItem* item) const {
  auto iter = std::find_if(window_list_.begin(), window_list_.end(),
                           base::MatchesUniquePtr(item));
  DCHECK(iter != window_list_.end());
  return iter - window_list_.begin();
}

void OverviewGrid::AddDraggedWindowIntoOverviewOnDragEnd(
    aura::Window* dragged_window) {
  DCHECK(overview_session_);
  if (overview_session_->IsWindowInOverview(dragged_window))
    return;

  // Update the dragged window's bounds before adding it to overview. The
  // dragged window might have resized to a smaller size if the drag
  // happens on tab(s).
  if (wm::IsDraggingTabs(dragged_window)) {
    const gfx::Rect old_bounds = dragged_window->bounds();
    // We need to temporarily disable the dragged window's ability to merge
    // into another window when changing the dragged window's bounds, so
    // that the dragged window doesn't merge into another window because of
    // its changed bounds.
    dragged_window->SetProperty(ash::kCanAttachToAnotherWindowKey, false);
    TabletModeWindowState::UpdateWindowPosition(
        wm::GetWindowState(dragged_window), /*animate=*/false);
    const gfx::Rect new_bounds = dragged_window->bounds();
    if (old_bounds != new_bounds) {
      // It's for smoother animation.
      gfx::Transform transform =
          ScopedOverviewTransformWindow::GetTransformForRect(
              gfx::RectF(new_bounds), gfx::RectF(old_bounds));
      dragged_window->SetTransform(transform);
    }
    dragged_window->ClearProperty(ash::kCanAttachToAnotherWindowKey);
  }

  overview_session_->AddItem(dragged_window, /*reposition=*/false,
                             /*animate=*/false);
}

}  // namespace ash
