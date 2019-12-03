// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MULTI_STORE_PASSWORD_SAVE_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MULTI_STORE_PASSWORD_SAVE_MANAGER_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"

namespace password_manager {

// This class encapsulates the logic for Save/Update credentials in the case
// when both a profile and account store exists. In case the account store isn't
// available (e.g. the user is Syncing), it should introduce no behavioral
// difference to the PasswordSaveManagerImpl base class. After the launch of the
// account store feature, this class should be merged with
// PasswordSaveManagerImpl.
// TODO(crbug.com/1012203): It currently has the following limitations:
// 1. When a PSL matched entry exists, a non-PSL matched entry is silently added
//    to the correpsonding store. However, if a PSL matched entry exists in both
//    stores, it's only added to the account store.
// 2. There is no API to set the destination store from the PasswordFormManager.
//    This will be eventually required in order to communicate the user choice
//    from the UI.
// 3. (Un)Blacklisting is done always against the profile store.

class MultiStorePasswordSaveManager : public PasswordSaveManagerImpl {
 public:
  MultiStorePasswordSaveManager(std::unique_ptr<FormSaver> profile_form_saver,
                                std::unique_ptr<FormSaver> account_form_saver);
  ~MultiStorePasswordSaveManager() override;

  void SaveInternal(const std::vector<const autofill::PasswordForm*>& matches,
                    const base::string16& old_password) override;

  void UpdateInternal(const std::vector<const autofill::PasswordForm*>& matches,
                      const base::string16& old_password) override;

  void PermanentlyBlacklist(
      const PasswordStore::FormDigest& form_digest) override;
  void Unblacklist(const PasswordStore::FormDigest& form_digest) override;

 protected:
  FormSaver* GetFormSaverForGeneration() override;

 private:
  bool IsAccountStoreActive();

  const std::unique_ptr<FormSaver> account_store_form_saver_;
  DISALLOW_COPY_AND_ASSIGN(MultiStorePasswordSaveManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MULTI_STORE_PASSWORD_SAVE_MANAGER_H_
