// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_privacy_info_view.h"

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/privacy_container_view.h"
#include "ash/assistant/util/i18n_util.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

AssistantPrivacyInfoView::AssistantPrivacyInfoView(
    AppListViewDelegate* view_delegate,
    PrivacyContainerView* container)
    : PrivacyInfoView(IDS_APP_LIST_ASSISTANT_PRIVACY_INFO,
                      IDS_APP_LIST_LEARN_MORE),
      view_delegate_(view_delegate),
      container_(container) {}

AssistantPrivacyInfoView::~AssistantPrivacyInfoView() = default;

void AssistantPrivacyInfoView::CloseButtonPressed() {
  view_delegate_->MarkAssistantPrivacyInfoDismissed();
  container_->Update();
}

void AssistantPrivacyInfoView::LinkClicked() {
  constexpr char url[] = "https://support.google.com/chromebook?p=assistant";
  AssistantController::Get()->OpenUrl(
      assistant::util::CreateLocalizedGURL(url));
}

}  // namespace ash
