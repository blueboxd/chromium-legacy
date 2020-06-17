// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/suggested_content_info_view.h"

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/gurl.h"

namespace ash {

SuggestedContentInfoView::SuggestedContentInfoView(
    AppListViewDelegate* view_delegate,
    SearchResultPageView* search_result_page_view)
    : PrivacyInfoView(IDS_APP_LIST_SUGGESTED_CONTENT_INFO,
                      IDS_APP_LIST_MANAGE_SETTINGS),
      view_delegate_(view_delegate),
      search_result_page_view_(search_result_page_view) {}

SuggestedContentInfoView::~SuggestedContentInfoView() = default;

void SuggestedContentInfoView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  if (!IsCloseButton(sender))
    return;

  view_delegate_->MarkSuggestedContentInfoDismissed();
  search_result_page_view_->OnPrivacyInfoViewCloseButtonPressed();
}

void SuggestedContentInfoView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                      const gfx::Range& range,
                                                      int event_flags) {
  view_delegate_->MarkSuggestedContentInfoDismissed();
  constexpr char url[] = "chrome://os-settings/osPrivacy";
  NewWindowDelegate::GetInstance()->NewTabWithUrl(
      GURL(url), /*from_user_interaction=*/true);
}

}  // namespace ash
