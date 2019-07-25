// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_page_view.h"

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
#include "ash/assistant/ui/assistant_web_view.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/public/cpp/view_shadow.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/search_box/search_box_constants.h"
#include "ui/views/background.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/fill_layout.h"

namespace app_list {

namespace {

// The shadow elevation value for the shadow of the Assistant search box.
constexpr int kShadowElevation = 12;

int GetPreferredHeightForAppListState(AppListView* app_list_view) {
  auto app_list_view_state = app_list_view->app_list_state();
  switch (app_list_view_state) {
    case ash::AppListViewState::kHalf:
    case ash::AppListViewState::kFullscreenSearch:
      return ash::kMaxHeightEmbeddedDip;
    default:
      return ash::kMinHeightEmbeddedDip;
  }
}

}  // namespace

AssistantPageView::AssistantPageView(
    ash::AssistantViewDelegate* assistant_view_delegate,
    ContentsView* contents_view)
    : assistant_view_delegate_(assistant_view_delegate),
      contents_view_(contents_view),
      min_height_dip_(ash::kMinHeightEmbeddedDip) {
  InitLayout();

  // |assistant_view_delegate_| could be nullptr in test.
  if (assistant_view_delegate_)
    assistant_view_delegate_->AddUiModelObserver(this);
}

AssistantPageView::~AssistantPageView() {
  if (assistant_view_delegate_)
    assistant_view_delegate_->RemoveUiModelObserver(this);
}

void AssistantPageView::InitLayout() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  view_shadow_ = std::make_unique<ash::ViewShadow>(this, kShadowElevation);
  view_shadow_->SetRoundedCornerRadius(
      search_box::kSearchBoxBorderCornerRadiusSearchResult);

  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  if (assistant_view_delegate_) {
    assistant_main_view_ = new AssistantMainView(assistant_view_delegate_);
    AddChildView(assistant_main_view_);

    // Web view.
    assistant_web_view_ = new ash::AssistantWebView(assistant_view_delegate_);
    AddChildView(assistant_web_view_);

    // Update the view state based on the current UI mode.
    OnUiModeChanged(assistant_view_delegate_->GetUiModel()->ui_mode(),
                    /*due_to_interaction=*/false);
  }
}

const char* AssistantPageView::GetClassName() const {
  return "AssistantPageView";
}

gfx::Size AssistantPageView::CalculatePreferredSize() const {
  constexpr int width = ash::kPreferredWidthDip;
  return gfx::Size(width, GetHeightForWidth(width));
}

int AssistantPageView::GetHeightForWidth(int width) const {
  int preferred_height =
      GetPreferredHeightForAppListState(contents_view_->app_list_view());

  preferred_height = std::max(preferred_height, min_height_dip_);
  return GetChildViewPreferredHeight() > preferred_height
             ? ash::kMaxHeightEmbeddedDip
             : preferred_height;
}

void AssistantPageView::OnBoundsChanged(const gfx::Rect& prev_bounds) {
  if (!IsDrawn())
    return;

  // Until Assistant UI is closed, the view may grow in height but not shrink.
  min_height_dip_ = std::max(min_height_dip_, GetContentsBounds().height());
}

void AssistantPageView::RequestFocus() {
  if (!assistant_view_delegate_)
    return;

  switch (assistant_view_delegate_->GetUiModel()->ui_mode()) {
    case ash::AssistantUiMode::kLauncherEmbeddedUi:
      if (assistant_main_view_)
        assistant_main_view_->RequestFocus();
      break;
    case ash::AssistantUiMode::kWebUi:
      if (assistant_web_view_)
        assistant_web_view_->RequestFocus();
      break;
    case ash::AssistantUiMode::kMainUi:
    case ash::AssistantUiMode::kMiniUi:
      NOTREACHED();
      break;
  }
}

void AssistantPageView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  View::GetAccessibleNodeData(node_data);
  node_data->SetName(l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_WINDOW));
}

void AssistantPageView::ChildPreferredSizeChanged(views::View* child) {
  MaybeUpdateAppListState(child->GetPreferredSize().height());
  PreferredSizeChanged();

  // After layout events, focus can be lost so we need to explicitly request
  // on behalf of the child views.
  RequestFocus();
}

void AssistantPageView::ChildVisibilityChanged(views::View* child) {
  if (!child->GetVisible())
    return;

  MaybeUpdateAppListState(child->GetPreferredSize().height());
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

gfx::Rect AssistantPageView::GetPageBoundsForState(
    ash::AppListState state) const {
  gfx::Rect bounds = AppListPage::GetSearchBoxBounds();
  bounds.Offset((bounds.width() - ash::kPreferredWidthDip) / 2, 0);
  bounds.set_size(GetPreferredSize());
  return bounds;
}

views::View* AssistantPageView::GetFirstFocusableView() {
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), /*reverse=*/false, /*dont_loop=*/false);
}

views::View* AssistantPageView::GetLastFocusableView() {
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), /*reverse=*/true, /*dont_loop=*/false);
}

void AssistantPageView::OnUiModeChanged(ash::AssistantUiMode ui_mode,
                                        bool due_to_interaction) {
  for (auto* child : children())
    child->SetVisible(false);

  switch (ui_mode) {
    case ash::AssistantUiMode::kLauncherEmbeddedUi:
      if (assistant_main_view_)
        assistant_main_view_->SetVisible(true);
      break;
    case ash::AssistantUiMode::kWebUi:
      if (assistant_web_view_)
        assistant_web_view_->SetVisible(true);
      break;
    case ash::AssistantUiMode::kMainUi:
    case ash::AssistantUiMode::kMiniUi:
      NOTREACHED();
      break;
  }

  PreferredSizeChanged();
  RequestFocus();
}

void AssistantPageView::OnUiVisibilityChanged(
    ash::AssistantVisibility new_visibility,
    ash::AssistantVisibility old_visibility,
    base::Optional<ash::AssistantEntryPoint> entry_point,
    base::Optional<ash::AssistantExitPoint> exit_point) {
  if (!assistant_view_delegate_)
    return;

  if (new_visibility != ash::AssistantVisibility::kVisible) {
    min_height_dip_ = ash::kMinHeightEmbeddedDip;
    return;
  }

  const bool prefer_voice = assistant_view_delegate_->IsTabletMode() ||
                            assistant_view_delegate_->IsLaunchWithMicOpen();
  if (!ash::assistant::util::IsVoiceEntryPoint(entry_point.value(),
                                               prefer_voice)) {
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  }
}

int AssistantPageView::GetChildViewPreferredHeight() const {
  int height = 0;
  if (assistant_view_delegate_) {
    switch (assistant_view_delegate_->GetUiModel()->ui_mode()) {
      case ash::AssistantUiMode::kLauncherEmbeddedUi:
        if (assistant_main_view_)
          height = assistant_main_view_->GetPreferredSize().height();
        break;
      case ash::AssistantUiMode::kWebUi:
        if (assistant_web_view_)
          height = assistant_web_view_->GetPreferredSize().height();
        break;
      case ash::AssistantUiMode::kMainUi:
      case ash::AssistantUiMode::kMiniUi:
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
  if (app_list_view->app_list_state() != ash::AppListViewState::kPeeking)
    return;

  if (child_height > GetPreferredHeightForAppListState(app_list_view))
    app_list_view->SetState(ash::AppListViewState::kHalf);
}

}  // namespace app_list
