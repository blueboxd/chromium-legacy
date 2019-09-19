// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PASSWORD_UPDATE_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_PASSWORD_UPDATE_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "chrome/browser/android/password_edit_delegate.h"
#include "chrome/browser/android/password_editing_bridge.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

class Profile;

// An interface used for launching the entry editor and editing a credential
// record that already exists in the password store.
class PasswordUpdateDelegate : public PasswordEditDelegate,
                               public password_manager::PasswordStoreConsumer {
 public:
  PasswordUpdateDelegate(Profile* profile,
                         const autofill::PasswordForm& password_form);
  ~PasswordUpdateDelegate() override;

  void EditSavedPassword(const base::string16& new_username,
                         const base::string16& new_password) override;

 private:
  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  Profile* profile_ = nullptr;
  autofill::PasswordForm password_form_;
  base::string16 new_username_;
  base::string16 new_password_;
};

#endif  // CHROME_BROWSER_ANDROID_PASSWORD_UPDATE_DELEGATE_H_
