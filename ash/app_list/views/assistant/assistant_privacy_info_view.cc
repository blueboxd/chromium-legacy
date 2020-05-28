// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_privacy_info_view.h"

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/assistant/util/i18n_util.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

AssistantPrivacyInfoView::AssistantPrivacyInfoView(
    AppListViewDelegate* view_delegate,
    SearchResultPageView* search_result_page_view)
    : PrivacyInfoView(IDS_APP_LIST_ASSISTANT_PRIVACY_INFO),
      view_delegate_(view_delegate),
      search_result_page_view_(search_result_page_view) {}

AssistantPrivacyInfoView::~AssistantPrivacyInfoView() = default;

void AssistantPrivacyInfoView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  if (!IsCloseButton(sender))
    return;

  view_delegate_->MarkAssistantPrivacyInfoDismissed();
  search_result_page_view_->OnAssistantPrivacyInfoViewCloseButtonPressed();
}

void AssistantPrivacyInfoView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                      const gfx::Range& range,
                                                      int event_flags) {
  constexpr char url[] = "https://support.google.com/chromebook?p=assistant";
  AssistantController::Get()->OpenUrl(
      assistant::util::CreateLocalizedGURL(url));
}

}  // namespace ash
