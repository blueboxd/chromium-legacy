// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PRIVACY_INFO_VIEW_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PRIVACY_INFO_VIEW_H_

#include "ash/app_list/views/privacy_info_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/styled_label_listener.h"

namespace ash {

class AppListViewDelegate;
class SearchResultPageView;

// View representing Assistant's privacy info in the Launcher.
class AssistantPrivacyInfoView : public PrivacyInfoView {
 public:
  AssistantPrivacyInfoView(AppListViewDelegate* view_delegate,
                           SearchResultPageView* search_result_page_view);
  AssistantPrivacyInfoView(const AssistantPrivacyInfoView&) = delete;
  AssistantPrivacyInfoView& operator=(const AssistantPrivacyInfoView&) = delete;
  ~AssistantPrivacyInfoView() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

 private:
  AppListViewDelegate* const view_delegate_;
  SearchResultPageView* const search_result_page_view_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PRIVACY_INFO_VIEW_H_
