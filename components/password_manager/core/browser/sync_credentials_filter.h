// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_CREDENTIALS_FILTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_CREDENTIALS_FILTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/sync/driver/sync_service.h"

namespace signin {
class IdentityManager;
}

namespace password_manager {

// The sync- and GAIA- aware implementation of the filter.
class SyncCredentialsFilter : public CredentialsFilter {
 public:
  using SyncServiceFactoryFunction =
      base::RepeatingCallback<const syncer::SyncService*(void)>;
  using IdentityManagerFactoryFunction =
      base::RepeatingCallback<const signin::IdentityManager*(void)>;

  // Implements protection of sync credentials. Uses |client| to get the last
  // commited entry URL for a check against GAIA reauth site. Uses the factory
  // functions repeatedly to get the sync service and identity manager to pass
  // them to sync_util methods.
  SyncCredentialsFilter(
      const PasswordManagerClient* client,
      SyncServiceFactoryFunction sync_service_factory_function,
      IdentityManagerFactoryFunction identity_manager_factory_function);
  ~SyncCredentialsFilter() override;

  // CredentialsFilter
  bool ShouldSave(const autofill::PasswordForm& form) const override;
  bool ShouldSaveGaiaPasswordHash(
      const autofill::PasswordForm& form) const override;
  bool ShouldSaveEnterprisePasswordHash(
      const autofill::PasswordForm& form) const override;
  void ReportFormLoginSuccess(
      const PasswordFormManagerInterface& form_manager) const override;
  bool IsSyncAccountEmail(const std::string& username) const override;

 private:
  const PasswordManagerClient* const client_;

  const SyncServiceFactoryFunction sync_service_factory_function_;

  // For incognito profile, |identity_manager_factory_function_| returns the
  // sign in manager of its original profile.
  const IdentityManagerFactoryFunction identity_manager_factory_function_;

  DISALLOW_COPY_AND_ASSIGN(SyncCredentialsFilter);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_CREDENTIALS_FILTER_H_
