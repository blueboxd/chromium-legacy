// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/family_link_notice_screen_handler.h"

#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/family_link_notice_screen.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId FamilyLinkNoticeView::kScreenId;

FamilyLinkNoticeScreenHandler::FamilyLinkNoticeScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.FamilyLinkNoticeScreen.userActed");
}

FamilyLinkNoticeScreenHandler::~FamilyLinkNoticeScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void FamilyLinkNoticeScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  // TODO(crbug.com/1101318): provide translatable strings
  builder->Add("familyLinkDialogTitle", "Add parental controls after setup");
  builder->Add(
      "familyLinkDialogSubtitle",
      "Your child's account isn't set up for Family Link parental controls. "
      "You can add parental controls once you finish setup. You'll find "
      "information on parental controls in the Explore app.");
  builder->Add("familyLinkContinueButton", "Continue");
}

void FamilyLinkNoticeScreenHandler::Initialize() {}

void FamilyLinkNoticeScreenHandler::Show() {
  ShowScreen(kScreenId);
}
void FamilyLinkNoticeScreenHandler::Bind(FamilyLinkNoticeScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}
void FamilyLinkNoticeScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}
}  // namespace chromeos
