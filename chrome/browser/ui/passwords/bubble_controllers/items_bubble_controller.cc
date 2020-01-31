// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/items_bubble_controller.h"

#include "chrome/browser/password_manager/password_store_utils.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_store.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace metrics_util = password_manager::metrics_util;

namespace {

std::vector<autofill::PasswordForm> DeepCopyForms(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& forms) {
  std::vector<autofill::PasswordForm> result;
  result.reserve(forms.size());
  std::transform(forms.begin(), forms.end(), std::back_inserter(result),
                 [](const std::unique_ptr<autofill::PasswordForm>& form) {
                   return *form;
                 });
  return result;
}

}  // namespace

ItemsBubbleController::ItemsBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          std::move(delegate),
          /*display_disposition=*/metrics_util::MANUAL_MANAGE_PASSWORDS),
      dismissal_reason_(metrics_util::NO_DIRECT_INTERACTION) {
  local_credentials_ = DeepCopyForms(delegate_->GetCurrentForms());
  GetManagePasswordsDialogTitleText(GetWebContents()->GetVisibleURL(),
                                    delegate_->GetOrigin(),
                                    !local_credentials_.empty(), &title_);
}

ItemsBubbleController::~ItemsBubbleController() {
  if (!interaction_reported_)
    OnBubbleClosing();
}

void ItemsBubbleController::OnManageClicked(
    password_manager::ManagePasswordsReferrer referrer) {
  dismissal_reason_ = metrics_util::CLICKED_MANAGE;
  if (delegate_)
    delegate_->NavigateToPasswordManagerSettingsPage(referrer);
}

void ItemsBubbleController::OnPasswordAction(
    const autofill::PasswordForm& password_form,
    PasswordAction action) {
  Profile* profile = GetProfile();
  if (!profile)
    return;
  password_manager::PasswordStore* password_store =
      GetPasswordStore(profile, password_form.IsUsingAccountStore()).get();

  DCHECK(password_store);
  if (action == PasswordAction::kRemovePassword)
    password_store->RemoveLogin(password_form);
  else
    password_store->AddLogin(password_form);
}

void ItemsBubbleController::ReportInteractions() {
  metrics_util::LogGeneralUIDismissalReason(dismissal_reason_);
  // Record UKM statistics on dismissal reason.
  if (metrics_recorder_)
    metrics_recorder_->RecordUIDismissalReason(dismissal_reason_);
}

base::string16 ItemsBubbleController::GetTitle() const {
  return title_;
}
