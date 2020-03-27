// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_signin_notifier_impl.h"

#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"

namespace password_manager {

PasswordStoreSigninNotifierImpl::PasswordStoreSigninNotifierImpl(
    Profile* profile)
    : profile_(profile),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)) {
  DCHECK(identity_manager_);
}

PasswordStoreSigninNotifierImpl::~PasswordStoreSigninNotifierImpl() = default;

void PasswordStoreSigninNotifierImpl::SubscribeToSigninEvents(
    PasswordStore* store) {
  set_store(store);
  identity_manager_->AddObserver(this);
}

void PasswordStoreSigninNotifierImpl::UnsubscribeFromSigninEvents() {
  identity_manager_->RemoveObserver(this);
}

void PasswordStoreSigninNotifierImpl::NotifyUISignoutWillDeleteCredentials(
    const std::vector<autofill::PasswordForm>& unsynced_credentials) {
  // Find the last active tab and pass |unsynced_credentials| to the
  // ManagePasswordsUIController attached to it.
  Browser* browser = chrome::FindBrowserWithProfile(profile_);
  if (!browser)
    return;
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;
  auto* ui_controller =
      ManagePasswordsUIController::FromWebContents(web_contents);
  if (!ui_controller)
    return;
  ui_controller->NotifyUnsyncedCredentialsWillBeDeleted(unsynced_credentials);
}

void PasswordStoreSigninNotifierImpl::OnPrimaryAccountCleared(
    const CoreAccountInfo& account_info) {
  NotifySignedOut(account_info.email, /* primary_account= */ true);
}

// IdentityManager::Observer implementations.
void PasswordStoreSigninNotifierImpl::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  // Only reacts to content area (non-primary) Gaia account sign-out event.
  if (info.account_id != identity_manager_->GetPrimaryAccountId())
    NotifySignedOut(info.email, /* primary_account= */ false);
}

}  // namespace password_manager
