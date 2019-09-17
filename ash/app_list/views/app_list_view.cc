// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_view.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/horizontal_page_container.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_config_provider.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/wallpaper_types.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/skia_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/ime_util_chromeos.h"
#include "ui/wm/core/shadow_types.h"

using ash::ColorProfileType;

namespace app_list {

namespace {

// The height of the half app list from the bottom of the screen.
constexpr int kHalfAppListHeight = 545;

// The scroll offset in order to transition from PEEKING to FULLSCREEN
constexpr int kAppListMinScrollToSwitchStates = 20;

// The DIP distance from the bezel in which a gesture drag end results in a
// closed app list.
constexpr int kAppListBezelMargin = 50;

// The size of app info dialog in fullscreen app list.
constexpr int kAppInfoDialogWidth = 512;
constexpr int kAppInfoDialogHeight = 384;

// The duration of app list animations when |short_animations_for_testing| are
// enabled.
constexpr int kAppListAnimationDurationImmediateMs = 0;

// Events within this threshold from the top of the view will be reserved for
// home launcher gestures, if they can be processed.
constexpr int kAppListHomeLaucherGesturesThreshold = 32;

// Quality of the shield background blur.
constexpr float kAppListBlurQuality = 0.33f;

// Set animation durations to 0 for testing.
// TODO(oshima): Use ui::ScopedAnimationDurationScaleMode instead.
bool short_animations_for_testing;

// Histogram for the app list dragging in clamshell mode.
constexpr char kAppListDragInClamshellHistogram[] =
    "Apps.StateTransition.Drag.PresentationTime.ClamshellMode";
constexpr char kAppListDragInClamshellMaxLatencyHistogram[] =
    "Apps.StateTransition.Drag.PresentationTime.MaxLatency.ClamshellMode";

// Histogram for the app list dragging in tablet mode.
constexpr char kAppListDragInTabletHistogram[] =
    "Apps.StateTransition.Drag.PresentationTime.TabletMode";
constexpr char kAppListDragInTabletMaxLatencyHistogram[] =
    "Apps.StateTransition.Drag.PresentationTime.MaxLatency.TabletMode";

// Returns whether AppList's rounded corners should be hidden based on
// the app list view state and app list view bounds.
bool ShouldHideRoundedCorners(ash::AppListViewState app_list_state,
                              const gfx::Rect& bounds) {
  switch (app_list_state) {
    case ash::AppListViewState::kClosed:
      return false;
    case ash::AppListViewState::kFullscreenAllApps:
    case ash::AppListViewState::kFullscreenSearch:
      // Hide rounded corners in fullscreen state.
      return true;

    case ash::AppListViewState::kPeeking:
    case ash::AppListViewState::kHalf:
      // When the virtual keyboard shows, the AppListView is moved upward to
      // avoid the overlapping area with the virtual keyboard. As a result, its
      // bottom side may be on the display edge. Stop showing the rounded
      // corners under this circumstance.
      return bounds.y() == 0;
  }
  NOTREACHED();
  return false;
}

// This view forwards the focus to the search box widget by providing it as a
// FocusTraversable when a focus search is provided.
class SearchBoxFocusHost : public views::View {
 public:
  explicit SearchBoxFocusHost(views::Widget* search_box_widget)
      : search_box_widget_(search_box_widget) {}

  ~SearchBoxFocusHost() override {}

  views::FocusTraversable* GetFocusTraversable() override {
    return search_box_widget_;
  }

  // views::View:
  const char* GetClassName() const override { return "SearchBoxFocusHost"; }

 private:
  views::Widget* search_box_widget_;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxFocusHost);
};

SkColor GetBackgroundShieldColor(const std::vector<SkColor>& colors,
                                 float color_opacity) {
  const U8CPU sk_opacity_value = static_cast<U8CPU>(255 * color_opacity);

  const SkColor default_color = SkColorSetA(
      app_list::AppListView::kDefaultBackgroundColor, sk_opacity_value);

  if (colors.empty())
    return default_color;

  DCHECK_EQ(static_cast<size_t>(ColorProfileType::NUM_OF_COLOR_PROFILES),
            colors.size());
  const SkColor dark_muted =
      colors[static_cast<int>(ColorProfileType::DARK_MUTED)];
  if (SK_ColorTRANSPARENT == dark_muted)
    return default_color;

  return SkColorSetA(
      color_utils::GetResultingPaintColor(
          SkColorSetA(SK_ColorBLACK, AppListView::kAppListColorDarkenAlpha),
          dark_muted),
      sk_opacity_value);
}

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kExcludeWindowFromEventHandling, false)

// Gets radius for app list background corners when the app list has the
// provided height. The rounded corner should match the current app list height
// (so the rounded corners bottom edge matches the shelf top), until it reaches
// the app list background radius (i.e. background radius in peeking app list
// state).
// |height|: App list view height, relative to the shelf top (i.e. distance
//           between app list top and shelf top edge).
double GetBackgroundRadiusForAppListHeight(double height) {
  return std::min(
      static_cast<double>(AppListConfig::instance().background_radius()),
      std::max(height, 0.));
}

// This targeter prevents routing events to sub-windows, such as
// RenderHostWindow in order to handle events in context of app list.
class AppListEventTargeter : public aura::WindowTargeter {
 public:
  explicit AppListEventTargeter(AppListViewDelegate* delegate)
      : delegate_(delegate) {}
  ~AppListEventTargeter() override = default;

  // aura::WindowTargeter:
  bool SubtreeShouldBeExploredForEvent(aura::Window* window,
                                       const ui::LocatedEvent& event) override {
    if (delegate_ && !delegate_->CanProcessEventsOnApplistViews())
      return false;

    if (window->GetProperty(kExcludeWindowFromEventHandling)) {
      // Allow routing to sub-windows for ET_MOUSE_MOVED event which is used by
      // accessibility to enter the mode of exploration of WebView contents.
      if (event.type() != ui::ET_MOUSE_MOVED)
        return false;
    }

    if (window->GetProperty(ash::assistant::ui::kOnlyAllowMouseClickEvents)) {
      if (event.type() != ui::ET_MOUSE_PRESSED &&
          event.type() != ui::ET_MOUSE_RELEASED) {
        return false;
      }
    }

    return aura::WindowTargeter::SubtreeShouldBeExploredForEvent(window, event);
  }

 private:
  AppListViewDelegate* delegate_;  // Weak. Owned by AppListService.

  DISALLOW_COPY_AND_ASSIGN(AppListEventTargeter);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppListView::StateAnimationMetricsReporter

class AppListView::StateAnimationMetricsReporter
    : public ui::AnimationMetricsReporter {
 public:
  explicit StateAnimationMetricsReporter(AppListView* view) : view_(view) {}

  ~StateAnimationMetricsReporter() override = default;

  void SetTargetState(ash::AppListViewState target_state) {
    target_state_ = target_state;
  }

  void Start(bool is_in_tablet_mode) {
    is_in_tablet_mode_ = is_in_tablet_mode;
#if defined(DCHECK)
    DCHECK(!started_);
    started_ = ui::ScopedAnimationDurationScaleMode::duration_scale_mode() !=
               ui::ScopedAnimationDurationScaleMode::ZERO_DURATION;
#endif
  }

  void Reset();

  void SetTabletModeAnimationTransition(
      TabletModeAnimationTransition transition) {
    tablet_transition_ = transition;
  }

  // ui::AnimationMetricsReporter:
  void Report(int value) override;

 private:
  void RecordMetricsInTablet(int value);
  void RecordMetricsInClamshell(int value);

#if defined(DCHECK)
  bool started_ = false;
#endif
  base::Optional<ash::AppListViewState> target_state_;
  base::Optional<TabletModeAnimationTransition> tablet_transition_;
  bool is_in_tablet_mode_ = false;
  AppListView* view_;

  DISALLOW_COPY_AND_ASSIGN(StateAnimationMetricsReporter);
};

void AppListView::StateAnimationMetricsReporter::Reset() {
#if defined(DCHECK)
  started_ = false;
#endif
  tablet_transition_.reset();
  target_state_.reset();
}

void AppListView::StateAnimationMetricsReporter::Report(int value) {
  UMA_HISTOGRAM_PERCENTAGE("Apps.StateTransition.AnimationSmoothness", value);
  if (is_in_tablet_mode_)
    RecordMetricsInTablet(value);
  else
    RecordMetricsInClamshell(value);
  view_->OnStateTransitionAnimationCompleted();
  Reset();
}

void AppListView::StateAnimationMetricsReporter::RecordMetricsInTablet(
    int value) {
  // It can't ensure the target transition is properly set. Simply give up
  // reporting per-state metrics in that case. See https://crbug.com/954907.
  if (!tablet_transition_)
    return;
  switch (*tablet_transition_) {
    case TabletModeAnimationTransition::kDragReleaseShow:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.HomeLauncherTransition.AnimationSmoothness.DragReleaseShow",
          value);
      break;
    case TabletModeAnimationTransition::kDragReleaseHide:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.HomeLauncherTransition.AnimationSmoothness."
          "DragReleaseHide",
          value);
      break;
    case TabletModeAnimationTransition::kHomeButtonShow:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.HomeLauncherTransition.AnimationSmoothness."
          "PressAppListButtonShow",
          value);
      break;
    case TabletModeAnimationTransition::kHideHomeLauncherForWindow:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.HomeLauncherTransition.AnimationSmoothness."
          "HideLauncherForWindow",
          value);
      break;
    case TabletModeAnimationTransition::kEnterOverviewMode:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.HomeLauncherTransition.AnimationSmoothness.EnterOverview",
          value);
      break;
    case TabletModeAnimationTransition::kExitOverviewMode:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.HomeLauncherTransition.AnimationSmoothness.ExitOverview",
          value);
      break;
    case TabletModeAnimationTransition::kEnterFullscreenAllApps:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.HomeLauncherTransition.AnimationSmoothness."
          "EnterFullscreenAllApps",
          value);
      break;
    case TabletModeAnimationTransition::kEnterFullscreenSearch:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.HomeLauncherTransition.AnimationSmoothness."
          "EnterFullscreenSearch",
          value);
      break;
  }
}

void AppListView::StateAnimationMetricsReporter::RecordMetricsInClamshell(
    int value) {
  // It can't ensure the target transition is properly set. Simply give up
  // reporting per-state metrics in that case. See https://crbug.com/954907.
  if (!target_state_)
    return;
  switch (*target_state_) {
    case ash::AppListViewState::kClosed:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.StateTransition.AnimationSmoothness.Close.ClamshellMode",
          value);
      break;
    case ash::AppListViewState::kPeeking:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.StateTransition.AnimationSmoothness.Peeking.ClamshellMode",
          value);
      break;
    case ash::AppListViewState::kHalf:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.StateTransition.AnimationSmoothness.Half.ClamshellMode", value);
      break;
    case ash::AppListViewState::kFullscreenAllApps:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.StateTransition.AnimationSmoothness.FullscreenAllApps."
          "ClamshellMode",
          value);
      break;
    case ash::AppListViewState::kFullscreenSearch:
      UMA_HISTOGRAM_PERCENTAGE(
          "Apps.StateTransition.AnimationSmoothness.FullscreenSearch."
          "ClamshellMode",
          value);
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// An animation observer to notify AppListView of bounds animation completion or
// interruption.
class BoundsAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  explicit BoundsAnimationObserver(AppListView* view) : view_(view) {}

  ~BoundsAnimationObserver() override = default;

  void set_target_state(ash::AppListViewState target_state) {
    target_state_ = target_state;
  }

 private:
  // Overridden from ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    StopObservingImplicitAnimations();
    view_->OnBoundsAnimationCompleted();
    TRACE_EVENT_ASYNC_END1("ui", "AppList::StateTransitionAnimations", this,
                           "state", target_state_.value());
    target_state_ = base::nullopt;
  }

  AppListView* view_;
  base::Optional<ash::AppListViewState> target_state_;

  DISALLOW_COPY_AND_ASSIGN(BoundsAnimationObserver);
};

// The view for the app list background shield which changes color and radius.
class AppListBackgroundShieldView : public views::View {
 public:
  AppListBackgroundShieldView() : color_(AppListView::kDefaultBackgroundColor) {
    SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    layer()->SetFillsBoundsOpaquely(false);
    SetBackgroundRadius(AppListConfig::instance().background_radius());
    layer()->SetColor(color_);
  }

  ~AppListBackgroundShieldView() override = default;

  void UpdateBackground(bool use_blur) {
    if (blur_value_ == use_blur)
      return;
    blur_value_ = use_blur;

    if (use_blur) {
      layer()->SetBackgroundBlur(AppListConfig::instance().blur_radius());
      layer()->SetBackdropFilterQuality(kAppListBlurQuality);
    } else {
      layer()->SetBackgroundBlur(0);
    }
  }

  void UpdateBackgroundRadius(
      ash::AppListViewState state,
      bool shelf_has_rounded_corners,
      base::Optional<base::TimeTicks> animation_end_timestamp) {
    const double target_corner_radius =
        (state == ash::AppListViewState::kClosed && !shelf_has_rounded_corners)
            ? 0
            : AppListConfig::instance().background_radius();
    if (radius_ == target_corner_radius)
      return;

    layer()->GetAnimator()->StopAnimatingProperty(
        ui::LayerAnimationElement::ROUNDED_CORNERS);

    std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
    if (animation_end_timestamp.has_value()) {
      settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
          layer()->GetAnimator());
      settings->SetTransitionDuration((*animation_end_timestamp) -
                                      base::TimeTicks::Now());
      settings->SetTweenType(gfx::Tween::EASE_OUT);
      settings->SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
    }
    SetBackgroundRadius(target_corner_radius);
  }

  void SetBackgroundRadius(double radius) {
    radius_ = radius;
    layer()->SetRoundedCornerRadius({radius, radius, 0, 0});
  }

  void UpdateColor(SkColor color) {
    if (color_ == color)
      return;

    color_ = color;
    layer()->SetColor(color);
  }

  void UpdateBounds(const gfx::Rect& bounds) {
    // Inset bottom by 2 * the background radius to account for the rounded
    // corners on the top and bottom of the |app_list_background_shield_|. Only
    // add the inset to the bottom to keep padding at the top of the AppList the
    // same.
    gfx::Rect new_bounds = bounds;
    new_bounds.Inset(0, 0, 0,
                     -AppListConfig::instance().background_radius() * 2);
    SetBoundsRect(new_bounds);
  }

  SkColor GetColorForTest() const { return color_; }

  const char* GetClassName() const override {
    return "AppListBackgroundShieldView";
  }

 private:
  // Whether the background blur has been set on the background shield.
  bool blur_value_ = false;

  double radius_ = 0.f;

  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(AppListBackgroundShieldView);
};

// Animation used to translate AppListView as well as its child views. The
// location and opacity of child views' are updated in each animation frame so
// it is expensive.
class PeekingResetAnimation : public ui::LayerAnimationElement {
 public:
  PeekingResetAnimation(int y_offset,
                        base::TimeDelta duration_ms,
                        AppListView* view)
      : ui::LayerAnimationElement(ui::LayerAnimationElement::TRANSFORM,
                                  duration_ms),
        transform_(std::make_unique<ui::InterpolatedTranslation>(
            gfx::PointF(0, y_offset),
            gfx::PointF())),
        view_(view) {
    DCHECK(view_->is_in_drag());
    DCHECK_EQ(view_->app_list_state(), ash::AppListViewState::kPeeking);
    DCHECK(!view_->is_tablet_mode());
  }

  ~PeekingResetAnimation() override = default;

  // ui::LayerAnimationElement:
  void OnStart(ui::LayerAnimationDelegate* delegate) override {}
  bool OnProgress(double current,
                  ui::LayerAnimationDelegate* delegate) override {
    const double progress =
        gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT, current);
    delegate->SetTransformFromAnimation(
        transform_->Interpolate(progress),
        ui::PropertyChangeReason::FROM_ANIMATION);

    // Update child views' location and opacity at each animation frame because
    // child views' padding changes along with the app list view's bounds.
    view_->app_list_main_view()->contents_view()->UpdateYPositionAndOpacity();

    return true;
  }
  void OnGetTarget(TargetValue* target) const override {}
  void OnAbort(ui::LayerAnimationDelegate* delegate) override {}

 private:
  std::unique_ptr<ui::InterpolatedTransform> transform_;
  AppListView* view_;

  DISALLOW_COPY_AND_ASSIGN(PeekingResetAnimation);
};

////////////////////////////////////////////////////////////////////////////////
// AppListView::TestApi

AppListView::TestApi::TestApi(AppListView* view) : view_(view) {
  DCHECK(view_);
}

AppListView::TestApi::~TestApi() = default;

AppsGridView* AppListView::TestApi::GetRootAppsGridView() {
  return view_->GetRootAppsGridView();
}

////////////////////////////////////////////////////////////////////////////////
// AppListView:

AppListView::AppListView(AppListViewDelegate* delegate)
    : delegate_(delegate),
      model_(delegate->GetModel()),
      search_model_(delegate->GetSearchModel()),
      is_background_blur_enabled_(ash::features::IsBackgroundBlurEnabled()),
      bounds_animation_observer_(
          std::make_unique<BoundsAnimationObserver>(this)),
      state_animation_metrics_reporter_(
          std::make_unique<StateAnimationMetricsReporter>(this)) {
  CHECK(delegate);
}

AppListView::~AppListView() {
  // Remove child views first to ensure no remaining dependencies on delegate_.
  RemoveAllChildViews(true);
}

// static
void AppListView::ExcludeWindowFromEventHandling(aura::Window* window) {
  DCHECK(window);
  window->SetProperty(kExcludeWindowFromEventHandling, true);
}

// static
void AppListView::SetShortAnimationForTesting(bool enabled) {
  short_animations_for_testing = enabled;
}

// static
bool AppListView::ShortAnimationsForTesting() {
  return short_animations_for_testing;
}

void AppListView::InitView(bool is_tablet_mode, gfx::NativeView parent) {
  base::AutoReset<bool> auto_reset(&is_building_, true);
  time_shown_ = base::Time::Now();
  UpdateAppListConfig(parent);
  InitContents(is_tablet_mode);
  InitWidget(parent);
  InitChildWidget();
}

void AppListView::InitContents(bool is_tablet_mode) {
  DCHECK(!app_list_background_shield_);
  DCHECK(!app_list_main_view_);
  DCHECK(!search_box_view_);
  DCHECK(!announcement_view_);

  app_list_background_shield_ = new AppListBackgroundShieldView();
  app_list_background_shield_->UpdateBackground(/*use_blur*/ !is_tablet_mode &&
                                                is_background_blur_enabled_);
  AddChildView(app_list_background_shield_);

  app_list_main_view_ = new AppListMainView(delegate_, this);
  search_box_view_ = new SearchBoxView(app_list_main_view_, delegate_, this);
  search_box_view_->Init(is_tablet_mode);

  app_list_main_view_->Init(0, search_box_view_);
  AddChildView(app_list_main_view_);
  announcement_view_ = new views::View();
  AddChildView(announcement_view_);
}

void AppListView::InitWidget(gfx::NativeView parent) {
  DCHECK(!GetWidget());
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "AppList";
  params.parent = parent;
  params.delegate = this;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.layer_type = ui::LAYER_NOT_DRAWN;

  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));
  DCHECK_EQ(widget, GetWidget());
  widget->GetNativeWindow()->SetEventTargeter(
      std::make_unique<AppListEventTargeter>(delegate_));

  // Enable arrow key in FocusManager. Arrow left/right and up/down triggers
  // the same focus movement as tab/shift+tab.
  widget->GetFocusManager()->set_arrow_key_traversal_enabled_for_widget(true);

  widget->GetNativeView()->AddObserver(this);
}

void AppListView::InitChildWidget() {
  // Create a widget for the SearchBoxView to live in. This allows the
  // SearchBoxView to be on top of the custom launcher page's WebContents
  // (otherwise the search box events will be captured by the WebContents).
  views::Widget::InitParams search_box_widget_params(
      views::Widget::InitParams::TYPE_CONTROL);
  search_box_widget_params.parent = GetWidget()->GetNativeView();
  search_box_widget_params.opacity =
      views::Widget::InitParams::TRANSLUCENT_WINDOW;
  search_box_widget_params.name = "SearchBoxView";
  search_box_widget_params.delegate = search_box_view_;

  views::Widget* search_box_widget = new views::Widget;
  search_box_widget->Init(std::move(search_box_widget_params));
  DCHECK_EQ(search_box_widget, search_box_view_->GetWidget());

  // Assign an accessibility role to the native window of |search_box_widget|,
  // so that hitting search+right could move ChromeVox focus across search box
  // to other elements in app list view.
  search_box_widget->GetNativeWindow()->SetProperty(
      ui::kAXRoleOverride,
      static_cast<ax::mojom::Role>(ax::mojom::Role::kGroup));

  // The search box will not naturally receive focus by itself (because it is in
  // a separate widget). Create this SearchBoxFocusHost in the main widget to
  // forward the focus search into to the search box.
  SearchBoxFocusHost* search_box_focus_host =
      new SearchBoxFocusHost(search_box_widget);
  AddChildView(search_box_focus_host);
  search_box_widget->SetFocusTraversableParentView(search_box_focus_host);
  search_box_widget->SetFocusTraversableParent(
      GetWidget()->GetFocusTraversable());
}

void AppListView::Show(bool is_side_shelf, bool is_tablet_mode) {
  if (!time_shown_.has_value())
    time_shown_ = base::Time::Now();
  // The opacity of the AppListView may have been manipulated by overview mode,
  // so reset it before it is shown.
  GetWidget()->GetLayer()->SetOpacity(1.0f);
  is_side_shelf_ = is_side_shelf;

  app_list_main_view_->contents_view()->ResetForShow();

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_BROWSER_BACK, ui::EF_NONE));

  UpdateWidget();

  // The initial state is kPeeking. If tablet mode is enabled, a fullscreen
  // state will be set later.
  SetState(ash::AppListViewState::kPeeking);

  // Ensures that the launcher won't open underneath the a11y keyboard.
  CloseKeyboardIfVisible();

  OnTabletModeChanged(is_tablet_mode);
  app_list_main_view_->ShowAppListWhenReady();

  UMA_HISTOGRAM_TIMES(kAppListCreationTimeHistogram,
                      base::Time::Now() - time_shown_.value());
  time_shown_ = base::nullopt;
  RecordFolderMetrics();
}

void AppListView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  app_list_main_view_->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
}

void AppListView::Dismiss() {
  CloseKeyboardIfVisible();
  delegate_->DismissAppList();
}

void AppListView::CloseOpenedPage() {
  if (HandleCloseOpenFolder())
    return;

  HandleCloseOpenSearchBox();
}

bool AppListView::HandleCloseOpenFolder() {
  if (GetAppsContainerView()->IsInFolderView()) {
    GetAppsContainerView()->app_list_folder_view()->CloseFolderPage();
    return true;
  }
  return false;
}

bool AppListView::HandleCloseOpenSearchBox() {
  if (app_list_main_view_ &&
      app_list_main_view_->contents_view()->IsShowingSearchResults()) {
    return Back();
  }
  return false;
}

bool AppListView::Back() {
  return app_list_main_view_->contents_view()->Back();
}

void AppListView::OnPaint(gfx::Canvas* canvas) {
  views::WidgetDelegateView::OnPaint(canvas);
  if (!next_paint_callback_.is_null()) {
    next_paint_callback_.Run();
    next_paint_callback_.Reset();
  }
}

const char* AppListView::GetClassName() const {
  return "AppListView";
}

bool AppListView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  switch (accelerator.key_code()) {
    case ui::VKEY_ESCAPE:
    case ui::VKEY_BROWSER_BACK:
      // If the ContentsView does not handle the back action, then this is the
      // top level, so we close the app list.
      if (!Back() && !is_tablet_mode())
        Dismiss();
      break;
    default:
      NOTREACHED();
      return false;
  }

  // Don't let DialogClientView handle the accelerator.
  return true;
}

void AppListView::Layout() {
  // Avoid layout while building the view.
  if (is_building_)
    return;

  // Avoid layout during animations.
  if (GetWidget()->GetLayer()->GetAnimator() &&
      GetWidget()->GetLayer()->GetAnimator()->is_animating()) {
    return;
  }

  const gfx::Rect contents_bounds = GetContentsBounds();

  // Exclude the shelf height from the contents bounds to avoid apps grid from
  // overlapping with shelf.
  gfx::Rect main_bounds = contents_bounds;
  main_bounds.Inset(0, 0, 0, AppListConfig::instance().shelf_height());

  app_list_main_view_->SetBoundsRect(main_bounds);

  app_list_background_shield_->UpdateBounds(contents_bounds);

  UpdateAppListBackgroundYPosition(app_list_state_);
}

ax::mojom::Role AppListView::GetAccessibleWindowRole() {
  // Default role of root view is ax::mojom::Role::kWindow which traps ChromeVox
  // focus within the root view. Assign ax::mojom::Role::kGroup here to allow
  // the focus to move from elements in app list view to search box.
  return ax::mojom::Role::kGroup;
}

const AppListConfig& AppListView::GetAppListConfig() const {
  return *app_list_config_;
}

views::View* AppListView::GetAppListBackgroundShieldForTest() {
  return app_list_background_shield_;
}

SkColor AppListView::GetAppListBackgroundShieldColorForTest() {
  return app_list_background_shield_->GetColorForTest();
}

void AppListView::UpdateAppListConfig(aura::Window* parent_window) {
  const gfx::Size non_apps_grid_size = AppsContainerView::GetNonAppsGridSize();
  gfx::Size available_apps_grid_size = parent_window->bounds().size();
  available_apps_grid_size.Enlarge(
      -non_apps_grid_size.width(),
      -non_apps_grid_size.height() - AppListConfig::instance().shelf_height());

  // Create the app list configuration override if it's needed for the current
  // display bounds and the available apps grid size.
  std::unique_ptr<AppListConfig> new_config =
      AppListConfigProvider::Get().CreateForAppListWidget(
          display::Screen::GetScreen()
              ->GetDisplayNearestView(parent_window)
              .work_area()
              .size(),
          available_apps_grid_size, app_list_config_.get());

  if (!new_config)
    return;

  const bool is_initial_config = !app_list_config_.get();
  app_list_config_ = std::move(new_config);

  // Initial config should be set before the app list main view is initialized.
  DCHECK(!is_initial_config || !app_list_main_view_);

  // If the config changed, notify apps grids the config has changed.
  if (!is_initial_config) {
    GetFolderAppsGridView()->OnAppListConfigUpdated();
    GetRootAppsGridView()->OnAppListConfigUpdated();
  }
}

void AppListView::UpdateWidget() {
  // The widget's initial position will be off the bottom of the display.
  // Set native view's bounds directly to avoid screen position controller
  // setting bounds in the display where the widget has the largest
  // intersection.
  GetWidget()->GetNativeView()->SetBounds(
      GetPreferredWidgetBoundsForState(ash::AppListViewState::kClosed));
}

void AppListView::HandleClickOrTap(ui::LocatedEvent* event) {
  // If the virtual keyboard is visible, dismiss the keyboard and return early.
  if (CloseKeyboardIfVisible()) {
    search_box_view_->NotifyGestureEvent();
    return;
  }

  // Close embedded Assistant UI if it is shown.
  if (app_list_main_view()->contents_view()->IsShowingEmbeddedAssistantUI()) {
    Back();
    search_box_view_->ClearSearchAndDeactivateSearchBox();
    return;
  }

  // Clear focus if the located event is not handled by any child view.
  GetFocusManager()->ClearFocus();

  if (GetAppsContainerView()->IsInFolderView()) {
    // Close the folder if it is opened.
    GetAppsContainerView()->app_list_folder_view()->CloseFolderPage();
    return;
  }

  if ((event->IsGestureEvent() &&
       (event->AsGestureEvent()->type() == ui::ET_GESTURE_LONG_PRESS ||
        event->AsGestureEvent()->type() == ui::ET_GESTURE_LONG_TAP ||
        event->AsGestureEvent()->type() == ui::ET_GESTURE_TWO_FINGER_TAP)) ||
      (event->IsMouseEvent() &&
       event->AsMouseEvent()->IsOnlyRightMouseButton())) {
    // Don't show menus on empty areas of the AppListView in clamshell mode.
    if (!is_tablet_mode())
      return;

    // Home launcher is shown on top of wallpaper with trasparent background. So
    // trigger the wallpaper context menu for the same events.
    gfx::Point onscreen_location(event->location());
    ConvertPointToScreen(this, &onscreen_location);
    delegate_->ShowWallpaperContextMenu(
        onscreen_location, event->IsGestureEvent() ? ui::MENU_SOURCE_TOUCH
                                                   : ui::MENU_SOURCE_MOUSE);
    return;
  }

  if (!search_box_view_->is_search_box_active() &&
      model_->state() != ash::AppListState::kStateEmbeddedAssistant) {
    if (!is_tablet_mode())
      Dismiss();
    return;
  }

  search_box_view_->ClearSearchAndDeactivateSearchBox();
}

void AppListView::StartDrag(const gfx::Point& location) {
  // Convert drag point from widget coordinates to screen coordinates because
  // the widget bounds changes during the dragging.
  initial_drag_point_ = location;
  ConvertPointToScreen(this, &initial_drag_point_);
  initial_window_bounds_ = GetWidget()->GetWindowBoundsInScreen();
}

void AppListView::UpdateDrag(const gfx::Point& location) {
  // Update the widget bounds based on the initial widget bounds and drag delta.
  gfx::Point location_in_screen_coordinates = location;
  ConvertPointToScreen(this, &location_in_screen_coordinates);
  int new_y_position = location_in_screen_coordinates.y() -
                       initial_drag_point_.y() + initial_window_bounds_.y();

  UpdateYPositionAndOpacity(new_y_position,
                            GetAppListBackgroundOpacityDuringDragging());
}

void AppListView::EndDrag(const gfx::Point& location) {
  // Change the app list state based on where the drag ended. If fling velocity
  // was over the threshold, snap to the next state in the direction of the
  // fling.
  if (std::abs(last_fling_velocity_) >= kDragVelocityThreshold) {
    // If the user releases drag with velocity over the threshold, snap to
    // the next state, ignoring the drag release position.

    if (last_fling_velocity_ > 0) {
      switch (app_list_state_) {
        case ash::AppListViewState::kPeeking:
        case ash::AppListViewState::kHalf:
        case ash::AppListViewState::kFullscreenSearch:
        case ash::AppListViewState::kFullscreenAllApps:
          Dismiss();
          break;
        case ash::AppListViewState::kClosed:
          NOTREACHED();
          break;
      }
    } else {
      switch (app_list_state_) {
        case ash::AppListViewState::kFullscreenAllApps:
        case ash::AppListViewState::kFullscreenSearch:
          SetState(app_list_state_);
          break;
        case ash::AppListViewState::kHalf:
          SetState(ash::AppListViewState::kFullscreenSearch);
          break;
        case ash::AppListViewState::kPeeking:
          UMA_HISTOGRAM_ENUMERATION(kAppListPeekingToFullscreenHistogram,
                                    kSwipe, kMaxPeekingToFullscreen);
          SetState(ash::AppListViewState::kFullscreenAllApps);
          break;
        case ash::AppListViewState::kClosed:
          NOTREACHED();
          break;
      }
    }
  } else {
    const int fullscreen_height = GetFullscreenStateHeight();
    int app_list_height = 0;
    switch (app_list_state_) {
      case ash::AppListViewState::kFullscreenAllApps:
      case ash::AppListViewState::kFullscreenSearch:
        app_list_height = fullscreen_height;
        break;
      case ash::AppListViewState::kHalf:
        app_list_height = std::min(fullscreen_height, kHalfAppListHeight);
        break;
      case ash::AppListViewState::kPeeking:
        app_list_height = AppListConfig::instance().peeking_app_list_height();
        break;
      case ash::AppListViewState::kClosed:
        NOTREACHED();
        break;
    }

    const int app_list_threshold =
        app_list_height / kAppListThresholdDenominator;
    gfx::Point location_in_screen_coordinates = location;
    ConvertPointToScreen(this, &location_in_screen_coordinates);
    const int drag_delta =
        initial_drag_point_.y() - location_in_screen_coordinates.y();
    const int location_y_in_current_work_area =
        location_in_screen_coordinates.y() -
        GetDisplayNearestView().work_area().y();
    // If the drag ended near the bezel, close the app list.
    if (location_y_in_current_work_area >=
        (fullscreen_height - kAppListBezelMargin)) {
      Dismiss();
    } else {
      switch (app_list_state_) {
        case ash::AppListViewState::kFullscreenAllApps:
          if (drag_delta < -app_list_threshold) {
            if (is_tablet_mode_ || is_side_shelf_)
              Dismiss();
            else
              SetState(ash::AppListViewState::kPeeking);
          } else {
            SetState(app_list_state_);
          }
          break;
        case ash::AppListViewState::kFullscreenSearch:
          if (drag_delta < -app_list_threshold)
            Dismiss();
          else
            SetState(app_list_state_);
          break;
        case ash::AppListViewState::kHalf:
          if (drag_delta > app_list_threshold)
            SetState(ash::AppListViewState::kFullscreenSearch);
          else if (drag_delta < -app_list_threshold)
            Dismiss();
          else
            SetState(app_list_state_);
          break;
        case ash::AppListViewState::kPeeking:
          if (drag_delta > app_list_threshold) {
            SetState(ash::AppListViewState::kFullscreenAllApps);
            UMA_HISTOGRAM_ENUMERATION(kAppListPeekingToFullscreenHistogram,
                                      kSwipe, kMaxPeekingToFullscreen);
          } else if (drag_delta < -app_list_threshold) {
            Dismiss();
          } else {
            SetState(app_list_state_);
          }
          break;
        case ash::AppListViewState::kClosed:
          NOTREACHED();
          break;
      }
    }
  }
  SetIsInDrag(false);
  UpdateChildViewsYPositionAndOpacity();
  initial_drag_point_ = gfx::Point();
}

void AppListView::SetChildViewsForStateTransition(
    ash::AppListViewState target_state) {
  if (target_state != ash::AppListViewState::kPeeking &&
      target_state != ash::AppListViewState::kFullscreenAllApps &&
      target_state != ash::AppListViewState::kHalf &&
      target_state != ash::AppListViewState::kClosed) {
    return;
  }

  app_list_main_view_->contents_view()->OnAppListViewTargetStateChanged(
      target_state);

  if (target_state == ash::AppListViewState::kHalf)
    return;

  if (GetAppsContainerView()->IsInFolderView())
    GetAppsContainerView()->ResetForShowApps();

  // Do not update the contents view state on closing.
  if (target_state != ash::AppListViewState::kClosed) {
    app_list_main_view_->contents_view()->SetActiveState(
        ash::AppListState::kStateApps, !is_side_shelf_);
  }

  if (target_state == ash::AppListViewState::kPeeking) {
    // Set the apps to the initial page when PEEKING.
    ash::PaginationModel* pagination_model = GetAppsPaginationModel();
    if (pagination_model->total_pages() > 0 &&
        pagination_model->selected_page() != 0) {
      pagination_model->SelectPage(0, false /* animate */);
    }
  }
  if (target_state == ash::AppListViewState::kClosed && is_side_shelf_) {
    // Reset the search box to be shown again. This is done after the animation
    // is complete normally, but there is no animation when |is_side_shelf_|.
    search_box_view_->ClearSearchAndDeactivateSearchBox();
  }
}

void AppListView::ConvertAppListStateToFullscreenEquivalent(
    ash::AppListViewState* state) {
  if (!(is_side_shelf_ || is_tablet_mode_))
    return;

  // If side shelf or tablet mode are active, all transitions should be
  // made to the tablet mode/side shelf friendly versions.
  if (*state == ash::AppListViewState::kHalf) {
    *state = ash::AppListViewState::kFullscreenSearch;
  } else if (*state == ash::AppListViewState::kPeeking) {
    // FULLSCREEN_ALL_APPS->PEEKING in tablet/side shelf mode should close
    // instead of going to PEEKING.
    *state = app_list_state_ == ash::AppListViewState::kFullscreenAllApps
                 ? ash::AppListViewState::kClosed
                 : ash::AppListViewState::kFullscreenAllApps;
  }
}

void AppListView::MaybeIncreaseAssistantPrivacyInfoRowShownCount(
    ash::AppListViewState new_state) {
  AppListStateTransitionSource transition =
      GetAppListStateTransitionSource(new_state);
  switch (transition) {
    case kPeekingToHalf:
    case kFullscreenAllAppsToFullscreenSearch:
      if (app_list_main_view()->contents_view()->IsShowingSearchResults())
        delegate_->MaybeIncreaseAssistantPrivacyInfoShownCount();
      break;
    default:
      break;
  }
}

void AppListView::RecordStateTransitionForUma(ash::AppListViewState new_state) {
  AppListStateTransitionSource transition =
      GetAppListStateTransitionSource(new_state);
  // kMaxAppListStateTransition denotes a transition we are not interested in
  // recording (ie. PEEKING->PEEKING).
  if (transition == kMaxAppListStateTransition)
    return;

  UMA_HISTOGRAM_ENUMERATION(kAppListStateTransitionSourceHistogram, transition,
                            kMaxAppListStateTransition);

  switch (transition) {
    case kPeekingToFullscreenAllApps:
    case KHalfToFullscreenSearch:
      base::RecordAction(base::UserMetricsAction("AppList_PeekingToFull"));
      break;

    case kFullscreenAllAppsToPeeking:
      base::RecordAction(base::UserMetricsAction("AppList_FullToPeeking"));
      break;

    default:
      break;
  }
}

void AppListView::MaybeCreateAccessibilityEvent(
    ash::AppListViewState new_state) {
  if (new_state != ash::AppListViewState::kPeeking &&
      new_state != ash::AppListViewState::kFullscreenAllApps)
    return;

  base::string16 state_announcement;

  if (new_state == ash::AppListViewState::kPeeking) {
    state_announcement = l10n_util::GetStringUTF16(
        IDS_APP_LIST_SUGGESTED_APPS_ACCESSIBILITY_ANNOUNCEMENT);
  } else {
    state_announcement = l10n_util::GetStringUTF16(
        IDS_APP_LIST_ALL_APPS_ACCESSIBILITY_ANNOUNCEMENT);
  }
  announcement_view_->GetViewAccessibility().OverrideName(state_announcement);
  announcement_view_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

int AppListView::GetRemainingBoundsAnimationDistance() const {
  return GetWidget()->GetLayer()->transform().To2dTranslation().y();
}

display::Display AppListView::GetDisplayNearestView() const {
  return display::Screen::GetScreen()->GetDisplayNearestView(
      GetWidget()->GetNativeWindow()->parent());
}

AppsContainerView* AppListView::GetAppsContainerView() {
  return app_list_main_view_->contents_view()->GetAppsContainerView();
}

AppsGridView* AppListView::GetRootAppsGridView() {
  return GetAppsContainerView()->apps_grid_view();
}

AppsGridView* AppListView::GetFolderAppsGridView() {
  return GetAppsContainerView()->app_list_folder_view()->items_grid_view();
}

AppListStateTransitionSource AppListView::GetAppListStateTransitionSource(
    ash::AppListViewState target_state) const {
  switch (app_list_state_) {
    case ash::AppListViewState::kClosed:
      // CLOSED->X transitions are not useful for UMA.
      return kMaxAppListStateTransition;
    case ash::AppListViewState::kPeeking:
      switch (target_state) {
        case ash::AppListViewState::kClosed:
          return kPeekingToClosed;
        case ash::AppListViewState::kHalf:
          return kPeekingToHalf;
        case ash::AppListViewState::kFullscreenAllApps:
          return kPeekingToFullscreenAllApps;
        case ash::AppListViewState::kPeeking:
          // PEEKING->PEEKING is used when resetting the widget position after a
          // failed state transition. Not useful for UMA.
          return kMaxAppListStateTransition;
        case ash::AppListViewState::kFullscreenSearch:
          // PEEKING->FULLSCREEN_SEARCH is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
      }
    case ash::AppListViewState::kHalf:
      switch (target_state) {
        case ash::AppListViewState::kClosed:
          return kHalfToClosed;
        case ash::AppListViewState::kPeeking:
          return kHalfToPeeking;
        case ash::AppListViewState::kFullscreenSearch:
          return KHalfToFullscreenSearch;
        case ash::AppListViewState::kHalf:
          // HALF->HALF is used when resetting the widget position after a
          // failed state transition. Not useful for UMA.
          return kMaxAppListStateTransition;
        case ash::AppListViewState::kFullscreenAllApps:
          // HALF->FULLSCREEN_ALL_APPS is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
      }

    case ash::AppListViewState::kFullscreenAllApps:
      switch (target_state) {
        case ash::AppListViewState::kClosed:
          return kFullscreenAllAppsToClosed;
        case ash::AppListViewState::kPeeking:
          return kFullscreenAllAppsToPeeking;
        case ash::AppListViewState::kFullscreenSearch:
          return kFullscreenAllAppsToFullscreenSearch;
        case ash::AppListViewState::kHalf:
          // FULLSCREEN_ALL_APPS->HALF is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
        case ash::AppListViewState::kFullscreenAllApps:
          // FULLSCREEN_ALL_APPS->FULLSCREEN_ALL_APPS is used when resetting the
          // widget positon after a failed state transition. Not useful for UMA.
          return kMaxAppListStateTransition;
      }
    case ash::AppListViewState::kFullscreenSearch:
      switch (target_state) {
        case ash::AppListViewState::kClosed:
          return kFullscreenSearchToClosed;
        case ash::AppListViewState::kFullscreenAllApps:
          return kFullscreenSearchToFullscreenAllApps;
        case ash::AppListViewState::kFullscreenSearch:
          // FULLSCREEN_SEARCH->FULLSCREEN_SEARCH is used when resetting the
          // widget position after a failed state transition. Not useful for
          // UMA.
          return kMaxAppListStateTransition;
        case ash::AppListViewState::kPeeking:
          // FULLSCREEN_SEARCH->PEEKING is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
        case ash::AppListViewState::kHalf:
          // FULLSCREEN_SEARCH->HALF is not a valid transition.
          NOTREACHED();
          return kMaxAppListStateTransition;
      }
  }
}

views::View* AppListView::GetInitiallyFocusedView() {
  return app_list_main_view_->search_box_view()->search_box();
}

void AppListView::OnScrollEvent(ui::ScrollEvent* event) {
  if (!HandleScroll(gfx::Vector2d(event->x_offset(), event->y_offset()),
                    event->type())) {
    return;
  }

  event->SetHandled();
  event->StopPropagation();
}

void AppListView::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      event->SetHandled();
      if (is_in_drag_)
        return;
      initial_mouse_drag_point_ = event->location();
      break;
    case ui::ET_MOUSE_DRAGGED:
      event->SetHandled();
      if (is_side_shelf_ || is_tablet_mode_)
        return;
      if (!is_in_drag_ && event->IsOnlyLeftMouseButton()) {
        // Calculate the mouse drag offset to determine whether AppListView is
        // in drag.
        gfx::Vector2d drag_distance =
            event->location() - initial_mouse_drag_point_;
        if (abs(drag_distance.y()) < ash::kMouseDragThreshold)
          return;

        StartDrag(initial_mouse_drag_point_);
        SetIsInDrag(true);
        app_list_main_view_->contents_view()->UpdateYPositionAndOpacity();
      }

      if (!is_in_drag_)
        return;
      UpdateDrag(event->location());
      break;
    case ui::ET_MOUSE_RELEASED:
      event->SetHandled();
      initial_mouse_drag_point_ = gfx::Point();
      if (!is_in_drag_) {
        HandleClickOrTap(event);
        return;
      }
      EndDrag(event->location());
      CloseKeyboardIfVisible();
      SetIsInDrag(false);
      break;
    case ui::ET_MOUSEWHEEL:
      if (HandleScroll(event->AsMouseWheelEvent()->offset(), ui::ET_MOUSEWHEEL))
        event->SetHandled();
      break;
    default:
      break;
  }
}

void AppListView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_LONG_PRESS:
    case ui::ET_GESTURE_LONG_TAP:
    case ui::ET_GESTURE_TWO_FINGER_TAP:
      SetIsInDrag(false);
      event->SetHandled();
      HandleClickOrTap(event);
      break;
    case ui::ET_SCROLL_FLING_START:
    case ui::ET_GESTURE_SCROLL_BEGIN: {
      // If the search box is active when we start our drag, let it know.
      if (search_box_view_->is_search_box_active())
        search_box_view_->NotifyGestureEvent();

      if (event->location().y() < kAppListHomeLaucherGesturesThreshold) {
        if (delegate_->ProcessHomeLauncherGesture(event, gfx::Point())) {
          SetIsInDrag(false);
          event->SetHandled();
          HandleClickOrTap(event);
          return;
        }
      }

      // Avoid scrolling events for the app list in tablet mode.
      if (is_side_shelf_ || is_tablet_mode())
        return;
      // There may be multiple scroll begin events in one drag because the
      // relative location of the finger and widget is almost unchanged and
      // scroll begin event occurs when the relative location changes beyond a
      // threshold. So avoid resetting the initial drag point in drag.
      if (!is_in_drag_)
        StartDrag(event->location());
      SetIsInDrag(true);
      event->SetHandled();
      break;
    }
    case ui::ET_GESTURE_SCROLL_UPDATE: {
      gfx::Point location_in_screen = event->location();
      views::View::ConvertPointToScreen(this, &location_in_screen);
      if (delegate_->ProcessHomeLauncherGesture(event, location_in_screen)) {
        SetIsInDrag(true);
        event->SetHandled();
        return;
      }

      // Avoid scrolling events for the app list in tablet mode.
      if (is_side_shelf_ || is_tablet_mode())
        return;
      SetIsInDrag(true);
      last_fling_velocity_ = event->details().scroll_y();
      UpdateDrag(event->location());
      event->SetHandled();
      break;
    }
    case ui::ET_GESTURE_END: {
      gfx::Point location_in_screen = event->location();
      views::View::ConvertPointToScreen(this, &location_in_screen);
      if (delegate_->ProcessHomeLauncherGesture(event, location_in_screen)) {
        SetIsInDrag(false);
        event->SetHandled();
        return;
      }

      if (!is_in_drag_)
        break;
      // Avoid scrolling events for the app list in tablet mode.
      if (is_side_shelf_ || is_tablet_mode())
        return;
      EndDrag(event->location());
      event->SetHandled();
      break;
    }
    case ui::ET_MOUSEWHEEL: {
      if (HandleScroll(event->AsMouseWheelEvent()->offset(), ui::ET_MOUSEWHEEL))
        event->SetHandled();
      break;
    }
    default:
      break;
  }
}

void AppListView::OnKeyEvent(ui::KeyEvent* event) {
  RedirectKeyEventToSearchBox(event);
}

void AppListView::OnTabletModeChanged(bool started) {
  is_tablet_mode_ = started;

  search_box_view_->OnTabletModeChanged(started);
  search_model_->SetTabletMode(started);
  GetAppsContainerView()->OnTabletModeChanged(started);
  app_list_main_view_->contents_view()
      ->horizontal_page_container()
      ->OnTabletModeChanged(started);

  if (is_in_drag_) {
    SetIsInDrag(false);
    UpdateChildViewsYPositionAndOpacity();
  }

  // Refresh the state if the view is not in a fullscreen state.
  if (started && !is_fullscreen())
    SetState(app_list_state_);

  // In tablet mode, AppListView should not be moved because of the change in
  // virtual keyboard's visibility.
  if (started) {
    GetWidget()->GetNativeView()->ClearProperty(
        wm::kVirtualKeyboardRestoreBoundsKey);
  }

  app_list_background_shield_->UpdateBackground(
      /*use_blur*/ is_background_blur_enabled_ && !started);

  // Update background color opacity.
  SetBackgroundShieldColor();
}

void AppListView::OnWallpaperColorsChanged() {
  SetBackgroundShieldColor();
  search_box_view_->OnWallpaperColorsChanged();
}

bool AppListView::HandleScroll(const gfx::Vector2d& offset,
                               ui::EventType type) {
  // Ignore 0-offset events to prevent spurious dismissal, see crbug.com/806338
  // The system generates 0-offset ET_SCROLL_FLING_CANCEL events during simple
  // touchpad mouse moves. Those may be passed via mojo APIs and handled here.
  if ((offset.y() == 0 && offset.x() == 0) || is_in_drag() ||
      ShouldIgnoreScrollEvents()) {
    return false;
  }

  if (app_list_state_ != ash::AppListViewState::kPeeking &&
      app_list_state_ != ash::AppListViewState::kFullscreenAllApps) {
    return false;
  }

  // Let the Apps grid view handle the event first in FULLSCREEN_ALL_APPS.
  if (app_list_state_ == ash::AppListViewState::kFullscreenAllApps) {
    AppsGridView* apps_grid_view = GetAppsContainerView()->IsInFolderView()
                                       ? GetFolderAppsGridView()
                                       : GetRootAppsGridView();
    if (apps_grid_view->HandleScrollFromAppListView(offset, type))
      return true;
  }

  // The AppList should not be dismissed with scroll in tablet mode.
  if (is_tablet_mode())
    return true;

  // If the event is a mousewheel event, the offset is always large enough,
  // otherwise the offset must be larger than the scroll threshold.
  if (type == ui::ET_MOUSEWHEEL ||
      abs(offset.y()) > kAppListMinScrollToSwitchStates) {
    if (app_list_state_ == ash::AppListViewState::kFullscreenAllApps) {
      if (offset.y() > 0)
        Dismiss();
      return true;
    }

    SetState(ash::AppListViewState::kFullscreenAllApps);
    const AppListPeekingToFullscreenSource source =
        type == ui::ET_MOUSEWHEEL ? kMousewheelScroll : kMousepadScroll;
    UMA_HISTOGRAM_ENUMERATION(kAppListPeekingToFullscreenHistogram, source,
                              kMaxPeekingToFullscreen);
  }
  return true;
}

void AppListView::SetState(ash::AppListViewState new_state) {
  ash::AppListViewState new_state_override = new_state;
  ConvertAppListStateToFullscreenEquivalent(&new_state_override);
  MaybeCreateAccessibilityEvent(new_state_override);
  SetChildViewsForStateTransition(new_state_override);
  StartAnimationForState(new_state_override);
  MaybeIncreaseAssistantPrivacyInfoRowShownCount(new_state_override);
  RecordStateTransitionForUma(new_state_override);
  model_->SetStateFullscreen(new_state_override);
  app_list_state_ = new_state_override;

  if (!update_childview_each_frame_ &&
      app_list_state_ != ash::AppListViewState::kClosed) {
    // Layout is called on animation completion, which makes the child views
    // jump. Update y positions in advance here to avoid that.
    app_list_main_view_->contents_view()->UpdateYPositionAndOpacity();
  }

  if (GetWidget()->IsActive()) {
    // Reset the focus to initially focused view. This should be
    // done before updating visibility of views, because setting
    // focused view invisible automatically moves focus to next
    // focusable view, which potentially causes bugs.
    GetInitiallyFocusedView()->RequestFocus();
  }

  // Updates the visibility of app list items according to the change of
  // |app_list_state_|.
  GetAppsContainerView()->UpdateControlVisibility(app_list_state_, is_in_drag_);
}

void AppListView::StartAnimationForState(ash::AppListViewState target_state) {
  int animation_duration = kAppListAnimationDurationMs;
  if (ShortAnimationsForTesting() || is_side_shelf_ ||
      (target_state == ash::AppListViewState::kClosed &&
       delegate_->ShouldDismissImmediately())) {
    animation_duration = kAppListAnimationDurationImmediateMs;
  } else if (is_fullscreen() ||
             target_state == ash::AppListViewState::kFullscreenAllApps ||
             target_state == ash::AppListViewState::kFullscreenSearch) {
    // Animate over more time to or from a fullscreen state, to maintain a
    // similar speed.
    animation_duration = kAppListAnimationDurationFromFullscreenMs;
  }
  if (target_state == ash::AppListViewState::kClosed) {
    // On closing, animate the opacity twice as fast so the SearchBoxView does
    // not flash behind the shelf.
    const int child_animation_duration = animation_duration / 2;
    app_list_main_view_->contents_view()->FadeOutOnClose(
        base::TimeDelta::FromMilliseconds(child_animation_duration));
  } else {
    app_list_main_view_->contents_view()->FadeInOnOpen(
        base::TimeDelta::FromMilliseconds(animation_duration));
  }

  ApplyBoundsAnimation(target_state,
                       base::TimeDelta::FromMilliseconds(animation_duration));
}

void AppListView::ApplyBoundsAnimation(ash::AppListViewState target_state,
                                       base::TimeDelta duration_ms) {
  base::AutoReset<bool> auto_reset(
      &update_childview_each_frame_,
      ShouldUpdateChildViewsDuringAnimation(target_state));

  // Reset animation metrics reporter when animation is started.
  ResetTransitionMetricsReporter();

  ui::ImplicitAnimationObserver* animation_observer =
      delegate_->GetAnimationObserver(target_state);

  if (is_side_shelf_) {
    // There is no animation in side shelf.
    OnBoundsAnimationCompleted();
    UpdateAppListBackgroundYPosition(target_state);
    if (animation_observer)
      animation_observer->OnImplicitAnimationsCompleted();
    return;
  }

  if (is_tablet_mode_ && target_state != ash::AppListViewState::kClosed) {
    DCHECK(target_state == ash::AppListViewState::kFullscreenAllApps ||
           target_state == ash::AppListViewState::kFullscreenSearch);
    TabletModeAnimationTransition transition_type =
        target_state == ash::AppListViewState::kFullscreenAllApps
            ? TabletModeAnimationTransition::kEnterFullscreenAllApps
            : TabletModeAnimationTransition::kEnterFullscreenSearch;
    state_animation_metrics_reporter_->SetTabletModeAnimationTransition(
        transition_type);
  } else {
    state_animation_metrics_reporter_->SetTargetState(target_state);
  }

  gfx::Rect target_bounds = GetPreferredWidgetBoundsForState(target_state);

  // When closing the view should animate to the shelf bounds. The workspace
  // area will not reflect an autohidden shelf so ask for the proper bounds.
  const int y_for_closed_state =
      delegate_->GetTargetYForAppListHide(GetWidget()->GetNativeView());
  if (target_state == ash::AppListViewState::kClosed) {
    target_bounds.set_y(y_for_closed_state);
  }

  // Record the current transform before removing it because this bounds
  // animation could be pre-empting another bounds animation.
  ui::Layer* layer = GetWidget()->GetLayer();

  // Adjust the closed state y to account for auto-hidden shelf.
  const int current_bounds_y = app_list_state_ == ash::AppListViewState::kClosed
                                   ? y_for_closed_state
                                   : layer->bounds().y();
  const int current_y_with_transform =
      current_bounds_y + GetRemainingBoundsAnimationDistance();

  const gfx::Transform current_shield_transform =
      app_list_background_shield_->layer()->transform();

  // Schedule the animation; set to the target bounds, and make the transform
  // to make this appear in the original location. Then set an empty transform
  // with the animation.
  layer->SetBounds(target_bounds);

  gfx::Transform transform;
  const int y_offset = current_y_with_transform - target_bounds.y();
  transform.Translate(0, y_offset);
  layer->SetTransform(transform);
  animation_end_timestamp_ = base::TimeTicks::Now() + duration_ms;

  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
  animation.SetAnimationMetricsReporter(GetStateTransitionMetricsReporter());
  TRACE_EVENT_ASYNC_BEGIN0("ui", "AppList::StateTransitionAnimations",
                           bounds_animation_observer_.get());
  bounds_animation_observer_->set_target_state(target_state);
  animation.AddObserver(bounds_animation_observer_.get());
  if (animation_observer) {
    // The presenter supplies an animation observer on closing.
    DCHECK_EQ(ash::AppListViewState::kClosed, target_state);
    animation.AddObserver(animation_observer);
  }

  // In fullscreen state, or peeking state with restricted vertical space, the
  // background shield is translated upwards to ensure background radius is not
  // visible.
  // NOTE: layer->SetBounds() changes shield transform, so reset the transform
  // to the value before the |layer| bounds are set before starting the
  // animation.
  app_list_background_shield_->SetTransform(current_shield_transform);
  ui::ScopedLayerAnimationSettings shield_animation(
      app_list_background_shield_->layer()->GetAnimator());
  shield_animation.SetTransitionDuration(duration_ms);
  shield_animation.SetTweenType(gfx::Tween::EASE_OUT);
  shield_animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);

  gfx::Transform shield_transform;
  if (ShouldHideRoundedCorners(target_state, target_bounds)) {
    shield_transform.Translate(0,
                               -AppListConfig::instance().background_radius());
  }
  app_list_background_shield_->SetTransform(shield_transform);

  if (update_childview_each_frame_) {
    layer->GetAnimator()->StartAnimation(new ui::LayerAnimationSequence(
        std::make_unique<PeekingResetAnimation>(y_offset, duration_ms, this)));
    return;
  }

  animation.SetTransitionDuration(duration_ms);
  animation.SetTweenType(gfx::Tween::EASE_OUT);
  layer->SetTransform(gfx::Transform());

  // Schedule animations of the rounded corners. When running on linux
  // workstation, the rounded corner animation sometimes looks out-of-sync. This
  // does not happen on actual devices.

  // TODO(mukai): fix the out-of-sync problem.
  app_list_background_shield_->UpdateBackgroundRadius(
      target_state, shelf_has_rounded_corners_, animation_end_timestamp_);
}

void AppListView::SetStateFromSearchBoxView(bool search_box_is_empty,
                                            bool triggered_by_contents_change) {
  switch (app_list_state_) {
    case ash::AppListViewState::kPeeking:
      if (app_list_features::IsZeroStateSuggestionsEnabled()) {
        if (!search_box_is_empty || search_box_view()->is_search_box_active())
          SetState(ash::AppListViewState::kHalf);
      } else {
        if (!search_box_is_empty)
          SetState(ash::AppListViewState::kHalf);
      }
      break;
    case ash::AppListViewState::kHalf:
      if (app_list_features::IsZeroStateSuggestionsEnabled()) {
        if (search_box_is_empty && !triggered_by_contents_change)
          SetState(ash::AppListViewState::kPeeking);
      } else {
        if (search_box_is_empty)
          SetState(ash::AppListViewState::kPeeking);
      }
      break;
    case ash::AppListViewState::kFullscreenSearch:
      if (app_list_features::IsZeroStateSuggestionsEnabled()) {
        if (search_box_is_empty && !triggered_by_contents_change) {
          SetState(ash::AppListViewState::kFullscreenAllApps);
          app_list_main_view()->contents_view()->SetActiveState(
              ash::AppListState::kStateApps);
        }
      } else {
        if (search_box_is_empty) {
          SetState(ash::AppListViewState::kFullscreenAllApps);
          app_list_main_view()->contents_view()->SetActiveState(
              ash::AppListState::kStateApps);
        }
      }
      break;
    case ash::AppListViewState::kFullscreenAllApps:
      if (app_list_features::IsZeroStateSuggestionsEnabled()) {
        if (!search_box_is_empty ||
            (search_box_is_empty && triggered_by_contents_change))
          SetState(ash::AppListViewState::kFullscreenSearch);
      } else {
        if (!search_box_is_empty)
          SetState(ash::AppListViewState::kFullscreenSearch);
      }
      break;
    case ash::AppListViewState::kClosed:
      // We clean search on app list close.
      break;
  }
}

void AppListView::UpdateYPositionAndOpacity(int y_position_in_screen,
                                            float background_opacity) {
  DCHECK(!is_side_shelf_);
  if (app_list_state_ == ash::AppListViewState::kClosed)
    return;

  if (GetWidget()->GetLayer()->GetAnimator()->IsAnimatingProperty(
          ui::LayerAnimationElement::TRANSFORM)) {
    GetWidget()->GetLayer()->GetAnimator()->StopAnimatingProperty(
        ui::LayerAnimationElement::TRANSFORM);
  }

  SetIsInDrag(true);

  presentation_time_recorder_->RequestNext();

  background_opacity_in_drag_ = background_opacity;
  gfx::Rect new_widget_bounds = GetWidget()->GetWindowBoundsInScreen();
  app_list_y_position_in_screen_ = std::min(
      std::max(y_position_in_screen, GetDisplayNearestView().work_area().y()),
      GetScreenBottom() - AppListConfig::instance().shelf_height());
  new_widget_bounds.set_y(app_list_y_position_in_screen_);
  gfx::NativeView native_view = GetWidget()->GetNativeView();
  ::wm::ConvertRectFromScreen(native_view->parent(), &new_widget_bounds);
  native_view->SetBounds(new_widget_bounds);
  UpdateChildViewsYPositionAndOpacity();
}

void AppListView::OffsetYPositionOfAppList(int offset) {
  gfx::NativeView native_view = GetWidget()->GetNativeView();
  gfx::Transform transform;
  transform.Translate(0, offset);
  native_view->SetTransform(transform);
}

ash::PaginationModel* AppListView::GetAppsPaginationModel() {
  return GetRootAppsGridView()->pagination_model();
}

gfx::Rect AppListView::GetAppInfoDialogBounds() const {
  gfx::Rect app_info_bounds(GetDisplayNearestView().work_area());
  app_info_bounds.ClampToCenteredSize(
      gfx::Size(kAppInfoDialogWidth, kAppInfoDialogHeight));
  return app_info_bounds;
}

void AppListView::SetIsInDrag(bool is_in_drag) {
  // In tablet mode, |presentation_time_recorder_| is constructed/reset by
  // HomeLauncherGestureHandler.
  if (!is_in_drag && !is_tablet_mode_)
    presentation_time_recorder_.reset();

  if (is_in_drag == is_in_drag_)
    return;

  is_in_drag_ = is_in_drag;

  // Don't allow dragging to interrupt the close animation, it probably is not
  // intentional.
  if (app_list_state_ == ash::AppListViewState::kClosed)
    return;

  if (is_in_drag && !is_tablet_mode_) {
    presentation_time_recorder_.reset();
    presentation_time_recorder_ = ash::CreatePresentationTimeHistogramRecorder(
        GetWidget()->GetCompositor(), kAppListDragInClamshellHistogram,
        kAppListDragInClamshellMaxLatencyHistogram);
  }

  GetAppsContainerView()->UpdateControlVisibility(app_list_state_, is_in_drag_);
}

void AppListView::OnHomeLauncherGainingFocusWithoutAnimation() {
  if (GetFocusManager()->GetFocusedView() != GetInitiallyFocusedView())
    GetInitiallyFocusedView()->RequestFocus();

  if (GetAppsPaginationModel()->total_pages() > 0 &&
      GetAppsPaginationModel()->selected_page() != 0) {
    GetAppsPaginationModel()->SelectPage(0, false /* animate */);
  }

  app_list_main_view_->contents_view()->ResetForShow();
}

int AppListView::GetScreenBottom() const {
  return GetDisplayNearestView().bounds().bottom();
}

int AppListView::GetCurrentAppListHeight() const {
  if (!GetWidget())
    return AppListConfig::instance().shelf_height();
  return GetScreenBottom() - GetWidget()->GetWindowBoundsInScreen().y();
}

float AppListView::GetAppListTransitionProgress() const {
  const float current_height = GetCurrentAppListHeight();
  const float peeking_height =
      AppListConfig::instance().peeking_app_list_height();
  if (current_height <= peeking_height) {
    // Currently transition progress is between closed and peeking state.
    // Calculate the progress of this transition.
    const float shelf_height =
        GetScreenBottom() - GetDisplayNearestView().work_area().bottom();

    // When screen is rotated, the current height might be smaller than shelf
    // height for just one moment, which results in negative progress. So force
    // the progress to be non-negative.
    return std::max(0.0f, (current_height - shelf_height) /
                              (peeking_height - shelf_height));
  }

  // Currently transition progress is between peeking and fullscreen state.
  // Calculate the progress of this transition.
  const float fullscreen_height_above_peeking =
      GetFullscreenStateHeight() - peeking_height;
  const float current_height_above_peeking = current_height - peeking_height;
  DCHECK_GT(fullscreen_height_above_peeking, 0);
  DCHECK_LE(current_height_above_peeking, fullscreen_height_above_peeking);
  return 1 + current_height_above_peeking / fullscreen_height_above_peeking;
}

int AppListView::GetFullscreenStateHeight() const {
  const display::Display display = GetDisplayNearestView();
  const gfx::Rect display_bounds = display.bounds();
  return display_bounds.height() - display.work_area().y() + display_bounds.y();
}

ash::AppListViewState AppListView::CalculateStateAfterShelfDrag(
    const ui::LocatedEvent& event_in_screen,
    float launcher_above_shelf_bottom_amount) const {
  ash::AppListViewState app_list_state = ash::AppListViewState::kPeeking;
  if (event_in_screen.type() == ui::ET_SCROLL_FLING_START &&
      fabs(event_in_screen.AsGestureEvent()->details().velocity_y()) >
          kDragVelocityThreshold) {
    // If the scroll sequence terminates with a fling, show the fullscreen app
    // list if the fling was fast enough and in the correct direction, otherwise
    // close it.
    app_list_state =
        event_in_screen.AsGestureEvent()->details().velocity_y() < 0
            ? ash::AppListViewState::kFullscreenAllApps
            : ash::AppListViewState::kClosed;
  } else {
    // Snap the app list to corresponding state according to the snapping
    // thresholds.
    if (is_tablet_mode_) {
      app_list_state =
          launcher_above_shelf_bottom_amount > kDragSnapToFullscreenThreshold
              ? ash::AppListViewState::kFullscreenAllApps
              : ash::AppListViewState::kClosed;
    } else {
      if (launcher_above_shelf_bottom_amount <= kDragSnapToClosedThreshold)
        app_list_state = ash::AppListViewState::kClosed;
      else if (launcher_above_shelf_bottom_amount <=
               kDragSnapToPeekingThreshold)
        app_list_state = ash::AppListViewState::kPeeking;
      else
        app_list_state = ash::AppListViewState::kFullscreenAllApps;
    }
  }

  // Deal with the situation of dragging app list from shelf while typing in
  // the search box.
  if (app_list_state == ash::AppListViewState::kFullscreenAllApps) {
    ash::AppListState active_state =
        app_list_main_view_->contents_view()->GetActiveState();
    if (active_state == ash::AppListState::kStateSearchResults)
      app_list_state = ash::AppListViewState::kFullscreenSearch;
  }

  return app_list_state;
}

ui::AnimationMetricsReporter* AppListView::GetStateTransitionMetricsReporter() {
  state_animation_metrics_reporter_->Start(is_tablet_mode_);
  return state_animation_metrics_reporter_.get();
}

void AppListView::OnHomeLauncherDragStart() {
  DCHECK(!presentation_time_recorder_);
  presentation_time_recorder_ = ash::CreatePresentationTimeHistogramRecorder(
      GetWidget()->GetCompositor(), kAppListDragInTabletHistogram,
      kAppListDragInTabletMaxLatencyHistogram);
}

void AppListView::OnHomeLauncherDragInProgress() {
  DCHECK(presentation_time_recorder_);
  presentation_time_recorder_->RequestNext();
}

void AppListView::OnHomeLauncherDragEnd() {
  presentation_time_recorder_.reset();
}

void AppListView::ResetTransitionMetricsReporter() {
  state_animation_metrics_reporter_->Reset();
}

void AppListView::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(GetWidget()->GetNativeView(), window);
  window->RemoveObserver(this);
}

void AppListView::OnWindowBoundsChanged(aura::Window* window,
                                        const gfx::Rect& old_bounds,
                                        const gfx::Rect& new_bounds,
                                        ui::PropertyChangeReason reason) {
  DCHECK_EQ(GetWidget()->GetNativeView(), window);

  UpdateAppListConfig(window);

  gfx::Transform transform;
  if (ShouldHideRoundedCorners(app_list_state_, new_bounds))
    transform.Translate(0, -AppListConfig::instance().background_radius());

  // Avoid setting new transform if the shield is animating to (or already has)
  // the target value.
  if (app_list_background_shield_->layer()->GetTargetTransform() != transform) {
    app_list_background_shield_->SetTransform(transform);
    app_list_background_shield_->SchedulePaint();
  }
}

void AppListView::OnBoundsAnimationCompleted() {
  const bool was_animation_interrupted =
      GetRemainingBoundsAnimationDistance() != 0;

  if (app_list_state_ == ash::AppListViewState::kClosed) {
    // Close embedded Assistant UI if it is open, to reset the
    // |assistant_page_view| bounds and AppListState.
    auto* contents_view = app_list_main_view()->contents_view();
    if (contents_view->IsShowingEmbeddedAssistantUI())
      contents_view->ShowEmbeddedAssistantUI(false);
  }

  // Layout if the animation was completed.
  if (!was_animation_interrupted)
    Layout();
}

void AppListView::SetShelfHasRoundedCorners(bool shelf_has_rounded_corners) {
  if (shelf_has_rounded_corners_ == shelf_has_rounded_corners)
    return;
  shelf_has_rounded_corners_ = shelf_has_rounded_corners;
  base::Optional<base::TimeTicks> animation_end_timestamp;
  if (GetWidget() && GetWidget()->GetLayer()->GetAnimator()->is_animating()) {
    animation_end_timestamp = animation_end_timestamp_;
  }
  app_list_background_shield_->UpdateBackgroundRadius(
      app_list_state_, shelf_has_rounded_corners_, animation_end_timestamp);
}

void AppListView::UpdateChildViewsYPositionAndOpacity() {
  if (app_list_state_ == ash::AppListViewState::kClosed)
    return;

  UpdateAppListBackgroundYPosition(app_list_state_);

  // Update the opacity of the background shield.
  SetBackgroundShieldColor();

  search_box_view_->UpdateOpacity();
  app_list_main_view_->contents_view()->UpdateYPositionAndOpacity();
}

void AppListView::RedirectKeyEventToSearchBox(ui::KeyEvent* event) {
  if (event->handled())
    return;

  // Allow text input inside the Assistant page.
  if (app_list_main_view()->contents_view()->IsShowingEmbeddedAssistantUI())
    return;

  views::Textfield* search_box = search_box_view_->search_box();
  const bool is_search_box_focused = search_box->HasFocus();
  const bool is_folder_header_view_focused = GetAppsContainerView()
                                                 ->app_list_folder_view()
                                                 ->folder_header_view()
                                                 ->HasTextFocus();

  // Do not redirect the key event to the |search_box_| when focus is on a
  // text field.
  if (is_search_box_focused || is_folder_header_view_focused)
    return;

  // Do not redirect the arrow keys in app list as they are are used for focus
  // traversal and app movement.
  if (app_list_features::IsSearchBoxSelectionEnabled() &&
      IsArrowKeyEvent(*event) && !search_box_view_->is_search_box_active()) {
    return;
  }

  if (!app_list_features::IsSearchBoxSelectionEnabled() &&
      IsArrowKeyEvent(*event)) {
    return;
  }

  // Redirect key event to |search_box_|.
  search_box->OnKeyEvent(event);
  if (event->handled()) {
    // Set search box focused if the key event is consumed.
    search_box->RequestFocus();
    return;
  }

  // Insert it into search box if the key event is a character. Released
  // key should not be handled to prevent inserting duplicate character.
  if (event->type() == ui::ET_KEY_PRESSED)
    search_box->InsertChar(*event);
}

void AppListView::OnScreenKeyboardShown(bool shown) {
  if (onscreen_keyboard_shown_ == shown)
    return;

  onscreen_keyboard_shown_ = shown;
  if (shown && GetAppsContainerView()->IsInFolderView()) {
    // Move the app list up to prevent folders being blocked by the
    // on-screen keyboard.
    OffsetYPositionOfAppList(
        GetAppsContainerView()->app_list_folder_view()->GetYOffsetForFolder());
  } else {
    // If the keyboard is closing or a folder isn't being shown, reset
    // the app list's position
    OffsetYPositionOfAppList(0);
  }
  app_list_main_view_->contents_view()->NotifySearchBoxBoundsUpdated();
}

bool AppListView::CloseKeyboardIfVisible() {
  // TODO(ginko) abstract this function to be in
  // |keyboard::KeyboardUIController*|
  if (!keyboard::KeyboardUIController::HasInstance())
    return false;
  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  if (keyboard_controller->IsKeyboardVisible()) {
    keyboard_controller->HideKeyboardByUser();
    return true;
  }
  return false;
}

void AppListView::OnParentWindowBoundsChanged() {
  // Set the widget size to fit the new display metrics.
  GetWidget()->GetNativeView()->SetBounds(
      GetPreferredWidgetBoundsForState(app_list_state_));

  // Update the widget bounds to accomodate the new work
  // area.
  SetState(app_list_state_);
}

float AppListView::GetAppListBackgroundOpacityDuringDragging() {
  float top_of_applist = GetWidget()->GetWindowBoundsInScreen().y();
  const int shelf_height = AppListConfig::instance().shelf_height();
  float dragging_height =
      std::max((GetScreenBottom() - shelf_height - top_of_applist), 0.f);
  float coefficient =
      std::min(dragging_height / (kNumOfShelfSize * shelf_height), 1.0f);
  float shield_opacity =
      is_background_blur_enabled_ ? kAppListOpacityWithBlur : kAppListOpacity;
  // Assume shelf is opaque when start to drag down the launcher.
  const float shelf_opacity = 1.0f;
  return coefficient * shield_opacity + (1 - coefficient) * shelf_opacity;
}

void AppListView::SetBackgroundShieldColor() {
  // There is a chance when AppListView::OnWallpaperColorsChanged is called
  // from AppListViewDelegate, the |app_list_background_shield_| is not
  // initialized.
  if (!app_list_background_shield_)
    return;

  // Opacity is set on the color instead of the layer because changing opacity
  // of the layer changes opacity of the blur effect, which is not desired.
  float color_opacity = kAppListOpacity;

  if (is_tablet_mode_) {
    // The Homecher background should have an opacity of 0.
    color_opacity = 0;
  } else if (is_in_drag_) {
    // Allow a custom opacity while the AppListView is dragging to show a
    // gradual opacity change when dragging from the shelf.
    color_opacity = background_opacity_in_drag_;
  } else if (is_background_blur_enabled_) {
    color_opacity = kAppListOpacityWithBlur;
  }

  app_list_background_shield_->UpdateColor(GetBackgroundShieldColor(
      delegate_->GetWallpaperProminentColors(), color_opacity));
}

void AppListView::RecordFolderMetrics() {
  int number_of_apps_in_folders = 0;
  int number_of_folders = 0;
  AppListItemList* item_list =
      app_list_main_view_->model()->top_level_item_list();
  for (size_t i = 0; i < item_list->item_count(); ++i) {
    AppListItem* item = item_list->item_at(i);
    if (item->GetItemType() != AppListFolderItem::kItemType)
      continue;
    ++number_of_folders;
    AppListFolderItem* folder = static_cast<AppListFolderItem*>(item);
    if (folder->folder_type() == AppListFolderItem::FOLDER_TYPE_OEM)
      continue;  // Don't count items in OEM folders.
    number_of_apps_in_folders += folder->item_list()->item_count();
  }
  UMA_HISTOGRAM_COUNTS_100(kNumberOfFoldersHistogram, number_of_folders);
  UMA_HISTOGRAM_COUNTS_100(kNumberOfAppsInFoldersHistogram,
                           number_of_apps_in_folders);
}

bool AppListView::ShouldIgnoreScrollEvents() {
  // When the app list is doing state change animation or the apps grid view is
  // in transition, ignore the scroll events to prevent triggering extra state
  // changes or transtions.
  return GetWidget()->GetLayer()->GetAnimator()->is_animating() ||
         GetRootAppsGridView()->pagination_model()->has_transition();
}

int AppListView::GetPreferredWidgetYForState(
    ash::AppListViewState state) const {
  // Note that app list container fills the screen, so we can treat the
  // container's y as the top of display.
  const display::Display display = GetDisplayNearestView();
  const gfx::Rect work_area_bounds = display.work_area();
  switch (state) {
    case ash::AppListViewState::kPeeking:
      return display.bounds().height() -
             AppListConfig::instance().peeking_app_list_height();
    case ash::AppListViewState::kHalf:
      return std::max(work_area_bounds.y(),
                      display.bounds().height() - kHalfAppListHeight);
    case ash::AppListViewState::kFullscreenAllApps:
    case ash::AppListViewState::kFullscreenSearch:
      // The ChromeVox panel as well as the Docked Magnifier viewport affect the
      // workarea of the display. We need to account for that when applist is in
      // fullscreen to avoid being shown below them.
      return work_area_bounds.y() - display.bounds().y();
    case ash::AppListViewState::kClosed:
      // Align the widget y with shelf y to avoid flicker in show animation. In
      // side shelf mode, the widget y is the top of work area because the
      // widget does not animate.
      return (is_side_shelf_ ? work_area_bounds.y()
                             : work_area_bounds.bottom()) -
             display.bounds().y();
  }
}

gfx::Rect AppListView::GetPreferredWidgetBoundsForState(
    ash::AppListViewState state) {
  // Use parent's width instead of display width to avoid 1 px gap (See
  // https://crbug.com/884889).
  CHECK(GetWidget());
  aura::Window* parent = GetWidget()->GetNativeView()->parent();
  CHECK(parent);
  return delegate_->SnapBoundsToDisplayEdge(
      gfx::Rect(0, GetPreferredWidgetYForState(state), parent->bounds().width(),
                GetFullscreenStateHeight()));
}

void AppListView::UpdateAppListBackgroundYPosition(
    ash::AppListViewState state) {
  // Update the y position of the background shield.
  gfx::Transform transform;
  if (is_in_drag_) {
    float app_list_transition_progress = GetAppListTransitionProgress();
    if (app_list_transition_progress < 1 && !shelf_has_rounded_corners()) {
      const float shelf_height =
          GetScreenBottom() - GetDisplayNearestView().work_area().bottom();
      app_list_background_shield_->SetBackgroundRadius(
          GetBackgroundRadiusForAppListHeight(GetCurrentAppListHeight() -
                                              shelf_height));
    } else if (app_list_transition_progress >= 1 &&
               app_list_transition_progress <= 2) {
      // Translate background shield so that it ends drag at a y position
      // according to the background radius in peeking and fullscreen.
      transform.Translate(0, -AppListConfig::instance().background_radius() *
                                 (app_list_transition_progress - 1));
    }
  } else if (ShouldHideRoundedCorners(state, GetBoundsInScreen())) {
    transform.Translate(0, -AppListConfig::instance().background_radius());
  }

  // Avoid setting new transform if the shield is animating to (or already has)
  // the target value.
  if (app_list_background_shield_->layer()->GetTargetTransform() != transform)
    app_list_background_shield_->SetTransform(transform);
}

bool AppListView::ShouldUpdateChildViewsDuringAnimation(
    ash::AppListViewState target_state) const {
  // Return true when following conditions are all satisfied:
  // (1) In Clamshell mode.
  // (2) AppListView is in drag and both the current state and the target state
  // are Peeking. For example, drag AppListView from Peeking state by a little
  // offset then release the drag.
  // (3) The top of AppListView's current bounds is lower than that of
  // AppList in Peeking state. Because UI janks obviously in this situation
  // wihtout updating child views in each animation frame.

  if (is_tablet_mode_)
    return false;

  if (target_state != ash::AppListViewState::kPeeking || !is_in_drag_ ||
      app_list_state_ != target_state) {
    return false;
  }

  return GetWidget()->GetNativeView()->bounds().origin().y() >
         GetPreferredWidgetYForState(target_state);
}

void AppListView::OnStateTransitionAnimationCompleted() {
  delegate_->OnStateTransitionAnimationCompleted(app_list_state_);
}

void AppListView::OnTabletModeAnimationTransitionNotified(
    TabletModeAnimationTransition animation_transition) {
  state_animation_metrics_reporter_->SetTabletModeAnimationTransition(
      animation_transition);
}

void AppListView::EndDragFromShelf(ash::AppListViewState app_list_state) {
  if (app_list_state == ash::AppListViewState::kClosed ||
      app_list_state_ == ash::AppListViewState::kClosed) {
    Dismiss();
  } else {
    SetState(app_list_state);
  }
  SetIsInDrag(false);
  UpdateChildViewsYPositionAndOpacity();
}

}  // namespace app_list
