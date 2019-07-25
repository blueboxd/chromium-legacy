// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_widget.h"

#include <utility>

#include "ash/animation/animation_change_type.h"
#include "ash/focus_cycler.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/overflow_bubble.h"
#include "ash/shelf/overflow_bubble_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_background_animator_observer.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/system/status_area_layout_manager.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

constexpr int kShelfBlurRadius = 30;
// The maximum size of the opaque layer during an "overshoot" (drag away from
// the screen edge).
constexpr int kShelfMaxOvershootHeight = 32;
constexpr float kShelfBlurQuality = 0.33f;

// Return the first or last focusable child of |root|.
views::View* FindFirstOrLastFocusableChild(views::View* root,
                                           bool find_last_child) {
  views::FocusSearch search(root, find_last_child /*cycle*/,
                            false /*accessibility_mode*/);
  views::FocusTraversable* dummy_focus_traversable;
  views::View* dummy_focus_traversable_view;
  return search.FindNextFocusableView(
      root,
      find_last_child ? views::FocusSearch::SearchDirection::kBackwards
                      : views::FocusSearch::SearchDirection::kForwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kSkipStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
      &dummy_focus_traversable, &dummy_focus_traversable_view);
}

}  // namespace

// The contents view of the Shelf. This view contains ShelfView and
// sizes it to the width of the shelf minus the size of the status area.
class ShelfWidget::DelegateView : public views::WidgetDelegate,
                                  public views::AccessiblePaneView,
                                  public ShelfBackgroundAnimatorObserver {
 public:
  explicit DelegateView(ShelfWidget* shelf);
  ~DelegateView() override;

  void set_focus_cycler(FocusCycler* focus_cycler) {
    focus_cycler_ = focus_cycler;
  }

  FocusCycler* focus_cycler() { return focus_cycler_; }

  void SetParentLayer(ui::Layer* layer);

  void set_default_last_focusable_child(bool default_last_focusable_child) {
    default_last_focusable_child_ = default_last_focusable_child;
  }

  // views::WidgetDelegate:
  void DeleteDelegate() override { delete this; }
  views::Widget* GetWidget() override { return View::GetWidget(); }
  const views::Widget* GetWidget() const override { return View::GetWidget(); }

  bool CanActivate() const override;
  void ReorderChildLayers(ui::Layer* parent_layer) override;
  void UpdateBackgroundBlur();
  void UpdateOpaqueBackground();
  // This will be called when the parent local bounds change.
  void OnBoundsChanged(const gfx::Rect& old_bounds) override;

  // views::AccessiblePaneView:
  views::View* GetDefaultFocusableChild() override;

  // ShelfBackgroundAnimatorObserver:
  void UpdateShelfBackground(SkColor color) override;

  SkColor GetShelfBackgroundColor() const;

 private:
  ShelfWidget* shelf_widget_;
  FocusCycler* focus_cycler_;
  // A background layer that may be visible depending on a
  // ShelfBackgroundAnimator.
  ui::Layer opaque_background_;

  // When true, the default focus of the shelf is the last focusable child.
  bool default_last_focusable_child_ = false;

  // Cache the state of the background blur so that it can be updated only
  // when necessary.
  bool background_is_currently_blurred_ = false;

  DISALLOW_COPY_AND_ASSIGN(DelegateView);
};

ShelfWidget::DelegateView::DelegateView(ShelfWidget* shelf_widget)
    : shelf_widget_(shelf_widget),
      focus_cycler_(nullptr),
      opaque_background_(ui::LAYER_SOLID_COLOR) {
  DCHECK(shelf_widget_);
  set_owned_by_client();  // Deleted by DeleteDelegate().

  SetLayoutManager(std::make_unique<views::FillLayout>());
  set_allow_deactivate_on_esc(true);

  UpdateOpaqueBackground();
}

ShelfWidget::DelegateView::~DelegateView() = default;

// static
bool ShelfWidget::IsUsingViewsShelf() {
  switch (Shell::Get()->session_controller()->GetSessionState()) {
    case session_manager::SessionState::ACTIVE:
      return true;
    // See https://crbug.com/798869.
    case session_manager::SessionState::OOBE:
    case session_manager::SessionState::LOGIN_PRIMARY:
      return true;
    case session_manager::SessionState::LOCKED:
    case session_manager::SessionState::LOGIN_SECONDARY:
      return switches::IsUsingViewsLock();
    case session_manager::SessionState::UNKNOWN:
    case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
      return features::IsViewsLoginEnabled();
  }
}

void ShelfWidget::DelegateView::SetParentLayer(ui::Layer* layer) {
  layer->Add(&opaque_background_);
  ReorderLayers();
}

bool ShelfWidget::DelegateView::CanActivate() const {
  // Allow activations coming from the overflow bubble if it is currently shown
  // and active.
  aura::Window* active_window = window_util::GetActiveWindow();
  aura::Window* bubble_window = nullptr;
  aura::Window* shelf_window = shelf_widget_->GetNativeWindow();
  if (shelf_widget_->IsShowingOverflowBubble()) {
    bubble_window = shelf_widget_->shelf_view_->overflow_bubble()
                        ->bubble_view()
                        ->GetWidget()
                        ->GetNativeWindow();
  }
  if (active_window &&
      (active_window == bubble_window || active_window == shelf_window)) {
    return true;
  }

  // Only allow activation from the focus cycler, not from mouse events, etc.
  return focus_cycler_ && focus_cycler_->widget_activating() == GetWidget();
}

void ShelfWidget::DelegateView::ReorderChildLayers(ui::Layer* parent_layer) {
  views::View::ReorderChildLayers(parent_layer);
  parent_layer->StackAtBottom(&opaque_background_);
}

void ShelfWidget::DelegateView::UpdateBackgroundBlur() {
  // Blur only if the background is visible.
  const bool should_blur_background =
      opaque_background_.visible() &&
      shelf_widget_->shelf_layout_manager()->ShouldBlurShelfBackground();
  if (should_blur_background == background_is_currently_blurred_)
    return;

  opaque_background_.SetBackgroundBlur(should_blur_background ? kShelfBlurRadius
                                                              : 0);
  opaque_background_.SetBackdropFilterQuality(kShelfBlurQuality);

  background_is_currently_blurred_ = should_blur_background;
}

void ShelfWidget::DelegateView::UpdateOpaqueBackground() {
  const gfx::Rect local_bounds = GetLocalBounds();
  gfx::Rect opaque_background_bounds = local_bounds;

  const Shelf* shelf = shelf_widget_->shelf();
  const ShelfBackgroundType background_type =
      shelf_widget_->GetBackgroundType();

  if (!opaque_background_.visible())
    opaque_background_.SetVisible(true);

  // Extend the opaque layer a little bit to handle "overshoot" gestures
  // gracefully (the user drags the shelf further than it can actually go).
  // That way:
  // 1) When the shelf has rounded corners, only two of them are visible,
  // 2) Even when the shelf is squared, it doesn't tear off the screen edge
  // when dragged away.
  // To achieve this, we extend the layer in the same direction where the shelf
  // is aligned (downwards for a bottom shelf, etc.).
  const int radius = ShelfConstants::shelf_size() / 2;
  // We can easily round only 2 corners out of 4 which means we don't need as
  // much extra shelf height.
  const int safety_margin = kShelfMaxOvershootHeight;
  opaque_background_bounds.Inset(
      -shelf->SelectValueForShelfAlignment(0, safety_margin, 0), 0,
      -shelf->SelectValueForShelfAlignment(0, 0, safety_margin),
      -shelf->SelectValueForShelfAlignment(safety_margin, 0, 0));

  // Show rounded corners except in maximized (which includes split view) mode.
  if (background_type == SHELF_BACKGROUND_MAXIMIZED) {
    opaque_background_.SetRoundedCornerRadius({0, 0, 0, 0});
  } else {
    opaque_background_.SetRoundedCornerRadius({
        shelf->SelectValueForShelfAlignment(radius, 0, radius),
        shelf->SelectValueForShelfAlignment(radius, radius, 0),
        shelf->SelectValueForShelfAlignment(0, radius, 0),
        shelf->SelectValueForShelfAlignment(0, 0, radius),
    });
    opaque_background_.AddCacheRenderSurfaceRequest();
  }
  opaque_background_.SetBounds(opaque_background_bounds);
  UpdateBackgroundBlur();
  SchedulePaint();
}

void ShelfWidget::DelegateView::OnBoundsChanged(const gfx::Rect& old_bounds) {
  UpdateOpaqueBackground();
}

views::View* ShelfWidget::DelegateView::GetDefaultFocusableChild() {
  if (!IsUsingViewsShelf())
    return GetFirstFocusableChild();

  if (shelf_widget_->login_shelf_view_->GetVisible()) {
    return FindFirstOrLastFocusableChild(shelf_widget_->login_shelf_view_,
                                         default_last_focusable_child_);
  } else {
    return shelf_widget_->shelf_view_->FindFirstOrLastFocusableChild(
        default_last_focusable_child_);
  }
}

void ShelfWidget::DelegateView::UpdateShelfBackground(SkColor color) {
  opaque_background_.SetColor(color);
  UpdateOpaqueBackground();
}

SkColor ShelfWidget::DelegateView::GetShelfBackgroundColor() const {
  return opaque_background_.background_color();
}

bool ShelfWidget::GetHitTestRects(aura::Window* target,
                                  gfx::Rect* hit_test_rect_mouse,
                                  gfx::Rect* hit_test_rect_touch) {
  // This should only get called when the login shelf is visible, i.e. not
  // during an active session. In an active session, hit test rects should be
  // calculated higher up in the class hierarchy by |EasyResizeWindowTargeter|.
  // When in OOBE or locked/login screen, let events pass through empty parts
  // of the shelf.
  DCHECK(login_shelf_view_->GetVisible());
  gfx::Rect login_view_button_bounds =
      login_shelf_view_->ConvertRectToWidget(login_shelf_view_->GetMirroredRect(
          login_shelf_view_->get_button_union_bounds()));
  aura::Window* source = login_shelf_view_->GetWidget()->GetNativeWindow();
  aura::Window::ConvertRectToTarget(source, target->parent(),
                                    &login_view_button_bounds);
  *hit_test_rect_mouse = login_view_button_bounds;
  *hit_test_rect_touch = login_view_button_bounds;
  return true;
}

ShelfWidget::ShelfWidget(aura::Window* shelf_container, Shelf* shelf)
    : shelf_(shelf),
      background_animator_(SHELF_BACKGROUND_DEFAULT,
                           shelf_,
                           Shell::Get()->wallpaper_controller()),
      shelf_layout_manager_(new ShelfLayoutManager(this, shelf)),
      delegate_view_(new DelegateView(this)),
      shelf_view_(new ShelfView(ShelfModel::Get(), shelf_)),
      login_shelf_view_(
          new LoginShelfView(RootWindowController::ForWindow(shelf_container)
                                 ->lock_screen_action_background_controller())),
      scoped_session_observer_(this) {
  DCHECK(shelf_container);
  DCHECK(shelf_);

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "ShelfWidget";
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.delegate = delegate_view_;
  params.parent = shelf_container;

  Init(params);

  // The shelf should not take focus when initially shown.
  set_focus_on_creation(false);
  SetContentsView(delegate_view_);
  delegate_view_->SetParentLayer(GetLayer());

  // The shelf view observes the shelf model and creates icons as items are
  // added to the model.
  shelf_view_->Init();
  GetContentsView()->AddChildView(shelf_view_);
  GetContentsView()->AddChildView(login_shelf_view_);

  shelf_layout_manager_->AddObserver(this);
  shelf_container->SetLayoutManager(shelf_layout_manager_);
  shelf_layout_manager_->InitObservers();
  background_animator_.PaintBackground(
      shelf_layout_manager_->GetShelfBackgroundType(),
      AnimationChangeType::IMMEDIATE);

  background_animator_.AddObserver(delegate_view_);
  shelf_->AddObserver(this);
}

ShelfWidget::~ShelfWidget() {
  // Must call Shutdown() before destruction.
  DCHECK(!status_area_widget_);
}

void ShelfWidget::Initialize() {
  // Sets initial session state to make sure the UI is properly shown.
  OnSessionStateChanged(Shell::Get()->session_controller()->GetSessionState());
  GetFocusManager()->set_arrow_key_traversal_enabled_for_widget(true);
}

void ShelfWidget::Shutdown() {
  // Shutting down the status area widget may cause some widgets (e.g. bubbles)
  // to close, so uninstall the ShelfLayoutManager event filters first. Don't
  // reset the pointer until later because other widgets (e.g. app list) may
  // access it later in shutdown.
  shelf_layout_manager_->PrepareForShutdown();

  Shell::Get()->focus_cycler()->RemoveWidget(status_area_widget_.get());
  status_area_widget_.reset();

  // Don't need to update the shelf background during shutdown.
  background_animator_.RemoveObserver(delegate_view_);
  shelf_->RemoveObserver(this);

  // Don't need to observe focus/activation during shutdown.
  Shell::Get()->focus_cycler()->RemoveWidget(this);
  SetFocusCycler(nullptr);
}

void ShelfWidget::CreateStatusAreaWidget(aura::Window* status_container) {
  DCHECK(status_container);
  DCHECK(!status_area_widget_);
  status_area_widget_ =
      std::make_unique<StatusAreaWidget>(status_container, shelf_);
  status_area_widget_->Initialize();
  Shell::Get()->focus_cycler()->AddWidget(status_area_widget_.get());
  status_container->SetLayoutManager(new StatusAreaLayoutManager(this));
}

ShelfBackgroundType ShelfWidget::GetBackgroundType() const {
  return background_animator_.target_background_type();
}

int ShelfWidget::GetBackgroundAlphaValue(
    ShelfBackgroundType background_type) const {
  return background_animator_.GetBackgroundAlphaValue(background_type);
}

void ShelfWidget::OnShelfAlignmentChanged() {
  // Check added for http://crbug.com/738011.
  CHECK(status_area_widget_);
  status_area_widget_->UpdateAfterShelfAlignmentChange();
  // This call will in turn trigger a call to delegate_view_->SchedulePaint().
  delegate_view_->UpdateOpaqueBackground();
}

void ShelfWidget::OnTabletModeChanged() {
  shelf_view_->OnTabletModeChanged();
}

void ShelfWidget::PostCreateShelf() {
  SetFocusCycler(Shell::Get()->focus_cycler());

  shelf_layout_manager_->LayoutShelf();
  shelf_layout_manager_->UpdateAutoHideState();
  ShowIfHidden();
}

bool ShelfWidget::IsShowingAppList() const {
  return GetHomeButton() && GetHomeButton()->IsShowingAppList();
}

bool ShelfWidget::IsShowingMenu() const {
  return shelf_view_->IsShowingMenu();
}

bool ShelfWidget::IsShowingOverflowBubble() const {
  return shelf_view_->IsShowingOverflowBubble();
}

void ShelfWidget::SetFocusCycler(FocusCycler* focus_cycler) {
  delegate_view_->set_focus_cycler(focus_cycler);
  if (focus_cycler)
    focus_cycler->AddWidget(this);
}

FocusCycler* ShelfWidget::GetFocusCycler() {
  return delegate_view_->focus_cycler();
}

gfx::Rect ShelfWidget::GetScreenBoundsOfItemIconForWindow(
    aura::Window* window) {
  ShelfID id = ShelfID::Deserialize(window->GetProperty(kShelfIDKey));
  if (id.IsNull())
    return gfx::Rect();

  gfx::Rect bounds(shelf_view_->GetIdealBoundsOfItemIcon(id));
  gfx::Point screen_origin;
  views::View::ConvertPointToScreen(shelf_view_, &screen_origin);
  return gfx::Rect(screen_origin.x() + bounds.x(),
                   screen_origin.y() + bounds.y(), bounds.width(),
                   bounds.height());
}

HomeButton* ShelfWidget::GetHomeButton() const {
  return shelf_view_->GetHomeButton();
}

BackButton* ShelfWidget::GetBackButton() const {
  return shelf_view_->GetBackButton();
}

app_list::ApplicationDragAndDropHost*
ShelfWidget::GetDragAndDropHostForAppList() {
  return shelf_view_;
}

void ShelfWidget::set_default_last_focusable_child(
    bool default_last_focusable_child) {
  delegate_view_->set_default_last_focusable_child(
      default_last_focusable_child);
}

void ShelfWidget::FocusFirstOrLastFocusableChild(bool last) {
  // This is only ever called during an active session.
  if (!shelf_view_->GetVisible())
    return;
  views::View* to_focus = shelf_view_->FindFirstOrLastFocusableChild(last);

  Shell::Get()->focus_cycler()->FocusWidget(to_focus->GetWidget());
  to_focus->GetFocusManager()->SetFocusedView(to_focus);
}

bool ShelfWidget::OnNativeWidgetActivationChanged(bool active) {
  if (!Widget::OnNativeWidgetActivationChanged(active))
    return false;
  if (active) {
    // Do not focus the default element if the widget activation came from the
    // overflow bubble's focus cycling. The setter of
    // |activated_from_overflow_bubble_| should handle focusing the correct
    // view.
    if (activated_from_overflow_bubble_) {
      activated_from_overflow_bubble_ = false;
    } else {
      delegate_view_->SetPaneFocusAndFocusDefault();
    }
  }
  return true;
}

void ShelfWidget::WillDeleteShelfLayoutManager() {
  shelf_layout_manager_->RemoveObserver(this);
  shelf_layout_manager_ = nullptr;
}

void ShelfWidget::OnBackgroundTypeChanged(ShelfBackgroundType background_type,
                                          AnimationChangeType change_type) {
  delegate_view_->UpdateOpaqueBackground();
}

void ShelfWidget::OnSessionStateChanged(session_manager::SessionState state) {
  // Do not show shelf widget:
  // * when views based shelf is disabled
  // * in UNKNOWN state - it might be called before shelf was initialized
  // * on secondary screens in states other than ACTIVE
  //
  // TODO(alemate): better handle show-hide for some UI screens:
  // https://crbug.com/935842
  // https://crbug.com/935844
  // https://crbug.com/935846
  // https://crbug.com/935847
  // https://crbug.com/935852
  // https://crbug.com/935853
  // https://crbug.com/935856
  // https://crbug.com/935857
  // https://crbug.com/935858
  // https://crbug.com/935860
  // https://crbug.com/935861
  // https://crbug.com/935863
  bool using_views_shelf = IsUsingViewsShelf();
  bool unknown_state = state == session_manager::SessionState::UNKNOWN;
  bool hide_on_secondary_screen = shelf_->ShouldHideOnSecondaryDisplay(state);
  if (!using_views_shelf || unknown_state || hide_on_secondary_screen) {
    HideIfShown();
  } else {
    switch (state) {
      case session_manager::SessionState::ACTIVE:
        login_shelf_view_->SetVisible(false);
        shelf_view_->SetVisible(true);
        break;
      case session_manager::SessionState::LOCKED:
      case session_manager::SessionState::LOGIN_SECONDARY:
        shelf_view_->SetVisible(false);
        login_shelf_view_->SetVisible(true);
        break;
      case session_manager::SessionState::OOBE:
        login_shelf_view_->SetVisible(true);
        shelf_view_->SetVisible(false);
        break;
      case session_manager::SessionState::LOGIN_PRIMARY:
      case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
        login_shelf_view_->SetVisible(true);
        shelf_view_->SetVisible(false);
        break;
      default:
        // session_manager::SessionState::UNKNOWN handled in if statement above.
        NOTREACHED();
    }
    ShowIfHidden();
  }
  login_shelf_view_->UpdateAfterSessionChange();
}

void ShelfWidget::OnUserSessionAdded(const AccountId& account_id) {
  login_shelf_view_->UpdateAfterSessionChange();
}

SkColor ShelfWidget::GetShelfBackgroundColor() const {
  return delegate_view_->GetShelfBackgroundColor();
}

void ShelfWidget::HideIfShown() {
  if (IsVisible())
    Hide();
}

void ShelfWidget::ShowIfHidden() {
  if (!IsVisible())
    Show();
}

void ShelfWidget::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED) {
    keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();

    // If the shelf receives the mouse pressing event, the RootView of the shelf
    // will reset the gesture handler. As a result, if the shelf is in drag
    // progress when the mouse is pressed, shelf will not receive the gesture
    // end event. So explicitly cancel the drag in this scenario.
    shelf_layout_manager_->CancelDragOnShelfIfInProgress();
  }

  views::Widget::OnMouseEvent(event);
}

void ShelfWidget::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP_DOWN)
    keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();
  views::Widget::OnGestureEvent(event);
}

}  // namespace ash
