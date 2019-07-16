// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_password_form_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/form_saver_impl.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_store.h"

using autofill::PasswordForm;

namespace password_manager {

CredentialManagerPasswordFormManager::CredentialManagerPasswordFormManager(
    PasswordManagerClient* client,
    std::unique_ptr<PasswordForm> saved_form,
    CredentialManagerPasswordFormManagerDelegate* delegate,
    std::unique_ptr<FormSaver> form_saver,
    std::unique_ptr<FormFetcher> form_fetcher)
    : NewPasswordFormManager(client,
                             std::move(saved_form),
                             std::move(form_fetcher),
                             (form_saver ? std::move(form_saver)
                                         : std::make_unique<FormSaverImpl>(
                                               client->GetPasswordStore()))),
      delegate_(delegate) {}

CredentialManagerPasswordFormManager::~CredentialManagerPasswordFormManager() =
    default;

void CredentialManagerPasswordFormManager::OnFetchCompleted() {
  NewPasswordFormManager::OnFetchCompleted();

  CreatePendingCredentials();

  // Notify the delegate. This might result in deleting |this|, while
  // OnFetchCompleted is being called from FormFetcherImpl, owned by |this|. If
  // done directly, once OnFetchCompleted returns, the FormFetcherImpl will be
  // used after free. Therefore the call is posted to a separate task.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&CredentialManagerPasswordFormManager::NotifyDelegate,
                     weak_factory_.GetWeakPtr()));
}

metrics_util::CredentialSourceType
CredentialManagerPasswordFormManager::GetCredentialSource() {
  return metrics_util::CredentialSourceType::kCredentialManagementAPI;
}

void CredentialManagerPasswordFormManager::NotifyDelegate() {
  delegate_->OnProvisionalSaveComplete();
}

}  // namespace password_manager
