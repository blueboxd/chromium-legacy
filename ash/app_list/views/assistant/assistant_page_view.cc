// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_page_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/assistant/assistant_main_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/view_shadow.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/search_box/search_box_constants.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/views/background.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// The shadow elevation value for the shadow of the Assistant search box.
constexpr int kShadowElevation = 12;

int GetPreferredHeightForAppListState(AppListView* app_list_view) {
  auto app_list_view_state = app_list_view->app_list_state();
  switch (app_list_view_state) {
    case AppListViewState::kHalf:
    case AppListViewState::kFullscreenSearch:
      return kMaxHeightEmbeddedDip;
    default:
      return kMinHeightEmbeddedDip;
  }
}

}  // namespace

AssistantPageView::AssistantPageView(
    AssistantViewDelegate* assistant_view_delegate,
    ContentsView* contents_view)
    : assistant_view_delegate_(assistant_view_delegate),
      contents_view_(contents_view),
      min_height_dip_(kMinHeightEmbeddedDip) {
  InitLayout();

  if (AssistantController::Get())  // May be |nullptr| in tests.
    assistant_controller_observer_.Add(AssistantController::Get());

  if (AssistantUiController::Get())  // May be |nullptr| in tests.
    assistant_ui_model_observer_.Add(AssistantUiController::Get());
}

AssistantPageView::~AssistantPageView() = default;

void AssistantPageView::InitLayout() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  view_shadow_ = std::make_unique<ViewShadow>(this, kShadowElevation);
  view_shadow_->SetRoundedCornerRadius(
      search_box::kSearchBoxBorderCornerRadiusSearchResult);

  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // |assistant_view_delegate_| could be nullptr in test.
  if (!assistant_view_delegate_)
    return;

  assistant_main_view_ = AddChildView(
      std::make_unique<AssistantMainView>(assistant_view_delegate_));
}

const char* AssistantPageView::GetClassName() const {
  return "AssistantPageView";
}

gfx::Size AssistantPageView::CalculatePreferredSize() const {
  constexpr int kWidth = kPreferredWidthDip;
  return contents_view_->AdjustSearchBoxSizeToFitMargins(
      gfx::Size(kWidth, GetHeightForWidth(kWidth)));
}

int AssistantPageView::GetHeightForWidth(int width) const {
  int preferred_height =
      GetPreferredHeightForAppListState(contents_view_->app_list_view());

  preferred_height = std::max(preferred_height, min_height_dip_);
  return GetChildViewHeightForWidth(width) > preferred_height
             ? kMaxHeightEmbeddedDip
             : preferred_height;
}

void AssistantPageView::OnBoundsChanged(const gfx::Rect& prev_bounds) {
  if (!IsDrawn())
    return;

  // Until Assistant UI is closed, the view may grow in height but not shrink.
  min_height_dip_ = std::max(min_height_dip_, GetContentsBounds().height());
}

void AssistantPageView::RequestFocus() {
  if (!AssistantUiController::Get())  // May be |nullptr| in tests.
    return;

  switch (AssistantUiController::Get()->GetModel()->ui_mode()) {
    case AssistantUiMode::kLauncherEmbeddedUi:
      if (assistant_main_view_)
        assistant_main_view_->RequestFocus();
      break;
    case AssistantUiMode::kAmbientUi:
      NOTREACHED();
      break;
  }
}

void AssistantPageView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  View::GetAccessibleNodeData(node_data);
  node_data->SetName(l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_WINDOW));
}

void AssistantPageView::ChildPreferredSizeChanged(views::View* child) {
  MaybeUpdateAppListState(child->GetHeightForWidth(width()));
  PreferredSizeChanged();
}

void AssistantPageView::ChildVisibilityChanged(views::View* child) {
  if (!child->GetVisible())
    return;

  MaybeUpdateAppListState(child->GetHeightForWidth(width()));
}

void AssistantPageView::VisibilityChanged(views::View* starting_from,
                                          bool is_visible) {
  if (starting_from == this && !is_visible)
    min_height_dip_ = kMinHeightEmbeddedDip;
}

void AssistantPageView::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      // Prevents closing the AppListView when a click event is not handled.
      event->StopPropagation();
      break;
    default:
      break;
  }
}

void AssistantPageView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_DOUBLE_TAP:
    case ui::ET_GESTURE_LONG_PRESS:
    case ui::ET_GESTURE_LONG_TAP:
    case ui::ET_GESTURE_TWO_FINGER_TAP:
      // Prevents closing the AppListView when a tap event is not handled.
      event->StopPropagation();
      break;
    default:
      break;
  }
}

void AssistantPageView::OnShown() {
  // The preferred size might be different from the previous time, so updating
  // to the correct size here.
  SetSize(CalculatePreferredSize());
}

void AssistantPageView::OnAnimationStarted(AppListState from_state,
                                           AppListState to_state) {
  UpdatePageBoundsForState(to_state, contents_view()->GetContentsBounds(),
                           contents_view()->GetSearchBoxBounds(to_state));
}

base::Optional<int> AssistantPageView::GetSearchBoxTop(
    AppListViewState view_state) const {
  if (view_state == AppListViewState::kPeeking ||
      view_state == AppListViewState::kHalf) {
    return AppListConfig::instance().search_box_fullscreen_top_padding();
  }
  // For other view states, return base::nullopt so the ContentsView
  // sets the default search box widget origin.
  return base::nullopt;
}

views::View* AssistantPageView::GetFirstFocusableView() {
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), /*reverse=*/false, /*dont_loop=*/false);
}

views::View* AssistantPageView::GetLastFocusableView() {
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), /*reverse=*/true, /*dont_loop=*/false);
}

void AssistantPageView::AnimateYPosition(AppListViewState target_view_state,
                                         const TransformAnimator& animator) {
  // Assistant page view may host native views for its content. The native view
  // hosts use view to widget coordinate conversion to calculate the native view
  // bounds, and thus depend on the view transform values.
  // Make sure the view is laid out before starting the transform animation so
  // native views are not placed according to interim, animated page transform
  // value.
  layer()->GetAnimator()->StopAnimatingProperty(
      ui::LayerAnimationElement::TRANSFORM);
  if (needs_layout())
    Layout();

  animator.Run(layer(), this);
  animator.Run(view_shadow_->shadow()->shadow_layer(), nullptr);
}

void AssistantPageView::UpdatePageOpacityForState(AppListState state,
                                                  float search_box_opacity,
                                                  bool restore_opacity) {
  layer()->SetOpacity(search_box_opacity);
}

gfx::Rect AssistantPageView::GetPageBoundsForState(
    AppListState state,
    const gfx::Rect& contents_bounds,
    const gfx::Rect& search_box_bounds) const {
  gfx::Rect bounds =
      gfx::Rect(gfx::Point(contents_bounds.x(), search_box_bounds.y()),
                GetPreferredSize());
  bounds.Offset((contents_bounds.width() - bounds.width()) / 2, 0);
  return bounds;
}

void AssistantPageView::OnAssistantControllerDestroying() {
  if (AssistantUiController::Get())  // May be |nullptr| in tests.
    assistant_ui_model_observer_.Remove(AssistantUiController::Get());

  if (AssistantController::Get())  // May be |nullptr| in tests.
    assistant_controller_observer_.Remove(AssistantController::Get());
}

void AssistantPageView::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  if (!assistant_view_delegate_)
    return;

  if (new_visibility != AssistantVisibility::kVisible) {
    min_height_dip_ = kMinHeightEmbeddedDip;
    return;
  }

  // Assistant page will get focus when widget shown.
  if (GetWidget() && GetWidget()->IsActive())
    RequestFocus();

  const bool prefer_voice =
      assistant_view_delegate_->IsTabletMode() ||
      AssistantState::Get()->launch_with_mic_open().value_or(false);
  if (!assistant::util::IsVoiceEntryPoint(entry_point.value(), prefer_voice)) {
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  }
}

int AssistantPageView::GetChildViewHeightForWidth(int width) const {
  int height = 0;
  if (AssistantUiController::Get()) {  // May be |nullptr| in tests.
    switch (AssistantUiController::Get()->GetModel()->ui_mode()) {
      case AssistantUiMode::kLauncherEmbeddedUi:
        if (assistant_main_view_)
          height = assistant_main_view_->GetHeightForWidth(width);
        break;
      case AssistantUiMode::kAmbientUi:
        NOTREACHED();
        break;
    }
  }
  return height;
}

void AssistantPageView::MaybeUpdateAppListState(int child_height) {
  auto* app_list_view = contents_view_->app_list_view();
  auto* widget = app_list_view->GetWidget();
  // |app_list_view| may not be initialized.
  if (!widget || !widget->IsVisible())
    return;

  // Update app list view state for |assistant_page_view_|.
  // Embedded Assistant Ui only has two sizes. The only states change is from
  // kPeeking to kHalf state.
  if (app_list_view->app_list_state() != AppListViewState::kPeeking)
    return;

  if (child_height > GetPreferredHeightForAppListState(app_list_view))
    app_list_view->SetState(AppListViewState::kHalf);
}

}  // namespace ash
