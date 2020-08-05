// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/family_link_notice_screen.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/wizard_context.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/family_link_notice_screen_handler.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user_manager.h"

namespace {
constexpr char kUserActionContinue[] = "continue";
}  // namespace

namespace chromeos {

std::string FamilyLinkNoticeScreen::GetResultString(Result result) {
  switch (result) {
    case Result::DONE:
      return "Done";
    case Result::SKIPPED:
      return BaseScreen::kNotApplicable;
  }
}

FamilyLinkNoticeScreen::FamilyLinkNoticeScreen(
    FamilyLinkNoticeView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(FamilyLinkNoticeView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  if (view_)
    view_->Bind(this);
}

FamilyLinkNoticeScreen::~FamilyLinkNoticeScreen() {
  if (view_)
    view_->Unbind();
}

void FamilyLinkNoticeScreen::OnViewDestroyed(FamilyLinkNoticeView* view) {
  if (view_ == view)
    view_ = nullptr;
}

bool FamilyLinkNoticeScreen::MaybeSkip(WizardContext* context) {
  if (features::IsChildSpecificSigninEnabled() && context->sign_in_as_child &&
      !ProfileManager::GetActiveUserProfile()->IsChild()) {
    return false;
  }
  exit_callback_.Run(Result::SKIPPED);
  return true;
}

void FamilyLinkNoticeScreen::ShowImpl() {
  if (!view_)
    return;
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (profile->GetProfilePolicyConnector()->IsManaged() &&
      !profile->IsChild()) {
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    view_->SetDomain(connector->GetEnterpriseDisplayDomain());
    view_->SetDisplayEmail(
        user_manager::UserManager::Get()->GetActiveUser()->GetDisplayEmail());
  } else {
    view_->SetIsNewGaiaAccount(context()->is_child_gaia_account_new);
  }
  view_->Show();
}

void FamilyLinkNoticeScreen::HideImpl() {}

void FamilyLinkNoticeScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionContinue) {
    exit_callback_.Run(Result::DONE);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

}  // namespace chromeos
