// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/views/app_list_page.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/macros.h"
#include "base/optional.h"

namespace ash {
class AssistantViewDelegate;
class AssistantWebView;
class ViewShadow;
}  // namespace ash

namespace app_list {

class AssistantMainView;
class ContentsView;

// The Assistant page for the app list.
class APP_LIST_EXPORT AssistantPageView : public AppListPage,
                                          public ash::AssistantUiModelObserver {
 public:
  AssistantPageView(ash::AssistantViewDelegate* assistant_view_delegate,
                    ContentsView* contents_view);
  ~AssistantPageView() override;

  void InitLayout();

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnBoundsChanged(const gfx::Rect& prev_bounds) override;
  void RequestFocus() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // AppListPage:
  gfx::Rect GetPageBoundsForState(ash::AppListState state) const override;
  views::View* GetFirstFocusableView() override;
  views::View* GetLastFocusableView() override;

  // AssistantUiModelObserver:
  void OnUiModeChanged(ash::AssistantUiMode ui_mode,
                       bool due_to_interaction) override;
  void OnUiVisibilityChanged(
      ash::AssistantVisibility new_visibility,
      ash::AssistantVisibility old_visibility,
      base::Optional<ash::AssistantEntryPoint> entry_point,
      base::Optional<ash::AssistantExitPoint> exit_point) override;

 private:
  int GetChildViewPreferredHeight() const;
  void MaybeUpdateAppListState(int child_height);
  gfx::Rect AddShadowBorderToBounds(const gfx::Rect& bounds) const;

  ash::AssistantViewDelegate* const assistant_view_delegate_;
  ContentsView* contents_view_;

  // Owned by the views hierarchy.
  AssistantMainView* assistant_main_view_ = nullptr;
  ash::AssistantWebView* assistant_web_view_ = nullptr;

  int min_height_dip_;

  std::unique_ptr<ash::ViewShadow> view_shadow_;

  DISALLOW_COPY_AND_ASSIGN(AssistantPageView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_
