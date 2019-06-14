// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/presenter/app_list_presenter_impl.h"

#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/transform.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/public/activation_client.h"

namespace app_list {
namespace {

inline ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

// Callback from the compositor when it presented a valid frame. Used to
// record UMA of input latency.
void DidPresentCompositorFrame(base::TimeTicks event_time_stamp,
                               bool is_showing,
                               const gfx::PresentationFeedback& feedback) {
  const base::TimeTicks present_time = feedback.timestamp;
  if (present_time.is_null() || event_time_stamp.is_null() ||
      present_time < event_time_stamp) {
    return;
  }
  const base::TimeDelta input_latency = present_time - event_time_stamp;
  if (is_showing) {
    UMA_HISTOGRAM_TIMES(kAppListShowInputLatencyHistogram, input_latency);
  } else {
    UMA_HISTOGRAM_TIMES(kAppListHideInputLatencyHistogram, input_latency);
  }
}

}  // namespace

AppListPresenterImpl::AppListPresenterImpl(
    std::unique_ptr<AppListPresenterDelegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK(delegate_);
  delegate_->SetPresenter(this);
}

AppListPresenterImpl::~AppListPresenterImpl() {
  Dismiss(base::TimeTicks());
  // Ensures app list view goes before the controller since pagination model
  // lives in the controller and app list view would access it on destruction.
  if (view_) {
    view_->GetAppsPaginationModel()->RemoveObserver(this);
    if (view_->GetWidget())
      view_->GetWidget()->CloseNow();
  }
}

aura::Window* AppListPresenterImpl::GetWindow() {
  return is_target_visibility_show_ && view_
             ? view_->GetWidget()->GetNativeWindow()
             : nullptr;
}

void AppListPresenterImpl::Show(int64_t display_id,
                                base::TimeTicks event_time_stamp) {
  if (is_target_visibility_show_) {
    // Launcher is always visible on the internal display when home launcher is
    // enabled in tablet mode.
    if (display_id != GetDisplayId() && !delegate_->IsTabletMode()) {
      Dismiss(event_time_stamp);
    }
    return;
  }

  if (!delegate_->GetRootWindowForDisplayId(display_id)) {
    LOG(ERROR) << "Root window does not exist for display: " << display_id;
    return;
  }

  is_target_visibility_show_ = true;
  RequestPresentationTime(display_id, event_time_stamp);

  if (!view_) {
    // Note |delegate_| outlives the AppListView.
    AppListView* view = new AppListView(delegate_->GetAppListViewDelegate());
    delegate_->Init(view, display_id);
    SetView(view);
  }
  delegate_->ShowForDisplay(display_id);

  NotifyTargetVisibilityChanged(GetTargetVisibility());
  NotifyVisibilityChanged(GetTargetVisibility(), display_id);
}

void AppListPresenterImpl::Dismiss(base::TimeTicks event_time_stamp) {
  if (!is_target_visibility_show_)
    return;

  // If the app list target visibility is shown, there should be an existing
  // view.
  DCHECK(view_);

  is_target_visibility_show_ = false;
  const int64_t display_id = GetDisplayId();
  RequestPresentationTime(display_id, event_time_stamp);

  // Hide the active window if it is a transient descendant of |view_|'s widget.
  aura::Window* window = view_->GetWidget()->GetNativeWindow();
  aura::Window* active_window =
      ::wm::GetActivationClient(window->GetRootWindow())->GetActiveWindow();
  if (active_window) {
    aura::Window* transient_parent =
        ::wm::TransientWindowManager::GetOrCreate(active_window)
            ->transient_parent();
    while (transient_parent) {
      if (window == transient_parent) {
        active_window->Hide();
        break;
      }
      transient_parent =
          ::wm::TransientWindowManager::GetOrCreate(transient_parent)
              ->transient_parent();
    }
  }

  // The dismissal may have occurred in response to the app list losing
  // activation. Otherwise, our widget is currently active. When the animation
  // completes we'll hide the widget, changing activation. If a menu is shown
  // before the animation completes then the activation change triggers the menu
  // to close. By deactivating now we ensure there is no activation change when
  // the animation completes and any menus stay open.
  if (view_->GetWidget()->IsActive())
    view_->GetWidget()->Deactivate();

  delegate_->OnClosing();
  ScheduleDismissAnimation();
  NotifyTargetVisibilityChanged(GetTargetVisibility());
  base::RecordAction(base::UserMetricsAction("Launcher_Dismiss"));
}

bool AppListPresenterImpl::HandleCloseOpenFolder() {
  return is_target_visibility_show_ && view_ && view_->HandleCloseOpenFolder();
}

bool AppListPresenterImpl::HandleCloseOpenSearchBox() {
  return view_ && view_->HandleCloseOpenSearchBox();
}

ash::ShelfAction AppListPresenterImpl::ToggleAppList(
    int64_t display_id,
    app_list::AppListShowSource show_source,
    base::TimeTicks event_time_stamp) {
  bool request_fullscreen = show_source == kSearchKeyFullscreen ||
                            show_source == kShelfButtonFullscreen;
  // Dismiss or show based on the target visibility because the show/hide
  // animation can be reversed.
  if (is_target_visibility_show_) {
    if (request_fullscreen) {
      if (view_->app_list_state() == ash::AppListViewState::kPeeking) {
        view_->SetState(ash::AppListViewState::kFullscreenAllApps);
        return ash::SHELF_ACTION_APP_LIST_SHOWN;
      } else if (view_->app_list_state() == ash::AppListViewState::kHalf) {
        view_->SetState(ash::AppListViewState::kFullscreenSearch);
        return ash::SHELF_ACTION_APP_LIST_SHOWN;
      }
    }
    Dismiss(event_time_stamp);
    return ash::SHELF_ACTION_APP_LIST_DISMISSED;
  }
  Show(display_id, event_time_stamp);
  if (request_fullscreen)
    view_->SetState(ash::AppListViewState::kFullscreenAllApps);
  return ash::SHELF_ACTION_APP_LIST_SHOWN;
}

bool AppListPresenterImpl::IsVisible() const {
  return view_ && view_->GetWidget()->IsVisible();
}

bool AppListPresenterImpl::GetTargetVisibility() const {
  return is_target_visibility_show_;
}

void AppListPresenterImpl::UpdateYPositionAndOpacity(int y_position_in_screen,
                                                     float background_opacity) {
  if (!is_target_visibility_show_)
    return;

  if (view_)
    view_->UpdateYPositionAndOpacity(y_position_in_screen, background_opacity);
}

void AppListPresenterImpl::EndDragFromShelf(
    ash::AppListViewState app_list_state) {
  if (view_)
    view_->EndDragFromShelf(app_list_state);
}

void AppListPresenterImpl::ProcessMouseWheelOffset(
    const gfx::Vector2d& scroll_offset_vector) {
  if (view_)
    view_->HandleScroll(scroll_offset_vector, ui::ET_MOUSEWHEEL);
}

void AppListPresenterImpl::UpdateYPositionAndOpacityForHomeLauncher(
    int y_position_in_screen,
    float opacity,
    UpdateHomeLauncherAnimationSettingsCallback callback) {
  if (!GetTargetVisibility())
    return;

  // Manipulate the layer which contains the expand arrow, suggestion chips and
  // apps grid in app_list_main_view, and the search box.
  ui::Layer* layer = view_->GetWidget()->GetNativeWindow()->layer();
  if (!delegate_->IsTabletMode()) {
    // In clamshell mode, set the opacity of the AppList immediately to
    // instantly hide it. Opacity of the AppList is reset when it is shown
    // again.
    layer->SetOpacity(opacity);
    return;
  }

  const gfx::Transform translation(1.f, 0.f, 0.f, 1.f, 0.f,
                                   static_cast<float>(y_position_in_screen));
  if (layer->GetAnimator()->is_animating()) {
    layer->GetAnimator()->StopAnimating();

    // Reset the animation metrics reporter when the animation is interrupted.
    view_->ResetTransitionMetricsReporter();
  }

  std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
  if (!callback.is_null()) {
    settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
        layer->GetAnimator());
    callback.Run(settings.get());
  }
  layer->SetOpacity(opacity);

  // Only record animation metrics for transformation animation. Because the
  // animation triggered by setting opacity should have the same metrics values
  // with the transformation animation.
  if (settings.get()) {
    settings->SetAnimationMetricsReporter(
        view_->GetStateTransitionMetricsReporter());
  }

  layer->SetTransform(translation);

  // Update child views' y positions to target state to avoid stale positions.
  view_->app_list_main_view()->contents_view()->UpdateYPositionAndOpacity();
}

void AppListPresenterImpl::ShowEmbeddedAssistantUI(bool show) {
  if (view_)
    view_->app_list_main_view()->contents_view()->ShowEmbeddedAssistantUI(show);
}

bool AppListPresenterImpl::IsShowingEmbeddedAssistantUI() const {
  if (view_) {
    return view_->app_list_main_view()
        ->contents_view()
        ->IsShowingEmbeddedAssistantUI();
  }

  return false;
}

void AppListPresenterImpl::SetExpandArrowViewVisibility(bool show) {
  if (view_) {
    view_->app_list_main_view()->contents_view()->SetExpandArrowViewVisibility(
        show);
  }
}

void AppListPresenterImpl::OnTabletModeChanged(bool started) {
  if (started) {
    if (GetTargetVisibility()) {
      DCHECK(IsVisible());
      view_->OnTabletModeChanged(true);
    }
    home_launcher_shown_ = GetWindow() && GetWindow()->HasFocus();
  } else {
    if (IsVisible())
      view_->OnTabletModeChanged(false);
    home_launcher_shown_ = false;
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, private:

void AppListPresenterImpl::SetView(AppListView* view) {
  DCHECK(view_ == nullptr);
  DCHECK(is_target_visibility_show_);

  view_ = view;
  views::Widget* widget = view_->GetWidget();
  widget->AddObserver(this);
  aura::client::GetFocusClient(widget->GetNativeView())->AddObserver(this);
  view_->GetAppsPaginationModel()->AddObserver(this);

  // Sync the |onscreen_keyboard_shown_| in case |view_| is not initiated when
  // the on-screen is shown.
  view_->set_onscreen_keyboard_shown(delegate_->GetOnScreenKeyboardShown());
}

void AppListPresenterImpl::ResetView() {
  if (!view_)
    return;

  views::Widget* widget = view_->GetWidget();
  widget->RemoveObserver(this);
  GetLayer(widget)->GetAnimator()->RemoveObserver(this);
  aura::client::GetFocusClient(widget->GetNativeView())->RemoveObserver(this);

  view_->GetAppsPaginationModel()->RemoveObserver(this);

  view_ = nullptr;
}

void AppListPresenterImpl::ScheduleDismissAnimation() {
  aura::Window* root_window =
      view_->GetWidget()->GetNativeView()->GetRootWindow();
  view_->StartCloseAnimation(dismiss_without_animation_
                                 ? base::TimeDelta::FromMilliseconds(0)
                                 : delegate_->GetVisibilityAnimationDuration(

                                       root_window, /*is_visible=*/false),
                             this);
}

int64_t AppListPresenterImpl::GetDisplayId() {
  views::Widget* widget = view_ ? view_->GetWidget() : nullptr;
  if (!widget)
    return display::kInvalidDisplayId;
  return display::Screen::GetScreen()
      ->GetDisplayNearestView(widget->GetNativeView())
      .id();
}

void AppListPresenterImpl::NotifyVisibilityChanged(bool visible,
                                                   int64_t display_id) {
  // Skip adjacent same changes.
  if (last_visible_ == visible && last_display_id_ == display_id)
    return;
  last_visible_ = visible;
  last_display_id_ = display_id;

  // Notify the Shell and its observers of the app list visibility change.
  delegate_->OnVisibilityChanged(visible, display_id);
}

void AppListPresenterImpl::NotifyTargetVisibilityChanged(bool visible) {
  // Skip adjacent same changes.
  if (last_target_visible_ == visible)
    return;
  last_target_visible_ = visible;

  delegate_->OnTargetVisibilityChanged(visible);
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl,  aura::client::FocusChangeObserver implementation:

void AppListPresenterImpl::OnWindowFocused(aura::Window* gained_focus,
                                           aura::Window* lost_focus) {
  if (view_ && is_target_visibility_show_) {
    aura::Window* applist_window = view_->GetWidget()->GetNativeView();
    const bool applist_gained_focus = applist_window->Contains(gained_focus);
    if (delegate_->IsTabletMode()) {
      if (applist_window->Contains(lost_focus)) {
        home_launcher_shown_ = false;
        HandleCloseOpenSearchBox();
      } else if (applist_gained_focus) {
        home_launcher_shown_ = true;
        view_->OnHomeLauncherGainingFocusWithoutAnimation();
      }
    }

    if (applist_gained_focus)
      base::RecordAction(base::UserMetricsAction("AppList_WindowFocused"));

    aura::Window* applist_container = applist_window->parent();
    if (applist_container->Contains(lost_focus) &&
        (!gained_focus || !applist_container->Contains(gained_focus)) &&
        !switches::ShouldNotDismissOnBlur() && !delegate_->IsTabletMode()) {
      Dismiss(base::TimeTicks());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, ui::ImplicitAnimationObserver implementation:

void AppListPresenterImpl::OnImplicitAnimationsCompleted() {
  StopObservingImplicitAnimations();

  // This class observes the closing animation only.
  NotifyVisibilityChanged(GetTargetVisibility(), GetDisplayId());

  if (is_target_visibility_show_) {
    view_->GetWidget()->Activate();
  } else {
    // Hide the widget so it can be re-shown without re-creating it.
    view_->GetWidget()->Hide();
    delegate_->OnClosed();
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, views::WidgetObserver implementation:

void AppListPresenterImpl::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(view_->GetWidget(), widget);
  if (is_target_visibility_show_)
    Dismiss(base::TimeTicks());
  ResetView();
}

void AppListPresenterImpl::OnWidgetDestroyed(views::Widget* widget) {
  delegate_->OnClosed();
}

void AppListPresenterImpl::OnWidgetVisibilityChanged(views::Widget* widget,
                                                     bool visible) {
  DCHECK_EQ(view_->GetWidget(), widget);
  NotifyVisibilityChanged(visible, GetDisplayId());
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, PaginationModelObserver implementation:

void AppListPresenterImpl::TotalPagesChanged() {}

void AppListPresenterImpl::SelectedPageChanged(int old_selected,
                                               int new_selected) {
  current_apps_page_ = new_selected;
}

void AppListPresenterImpl::TransitionStarted() {}

void AppListPresenterImpl::TransitionChanged() {}

void AppListPresenterImpl::TransitionEnded() {}

void AppListPresenterImpl::RequestPresentationTime(
    int64_t display_id,
    base::TimeTicks event_time_stamp) {
  if (event_time_stamp.is_null())
    return;
  aura::Window* root_window = delegate_->GetRootWindowForDisplayId(display_id);
  if (!root_window)
    return;
  ui::Compositor* compositor = root_window->layer()->GetCompositor();
  if (!compositor)
    return;
  compositor->RequestPresentationTimeForNextFrame(
      base::BindOnce(&DidPresentCompositorFrame, event_time_stamp,
                     is_target_visibility_show_));
}

}  // namespace app_list
