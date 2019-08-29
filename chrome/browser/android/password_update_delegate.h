// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PASSWORD_UPDATE_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_PASSWORD_UPDATE_DELEGATE_H_

#include <memory>

#include "chrome/browser/android/password_change_delegate.h"
#include "chrome/browser/android/password_editing_bridge.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store.h"

// An interface used for launching the entry editor and performing a
// change on a credential record that already exists in the password store.
class PasswordUpdateDelegate : public PasswordChangeDelegate {
 public:
  PasswordUpdateDelegate(password_manager::PasswordStore* store,
                         const autofill::PasswordForm& password_form);
  ~PasswordUpdateDelegate() override;

 private:
  autofill::PasswordForm password_form_;

  DISALLOW_COPY_AND_ASSIGN(PasswordUpdateDelegate);
};

// Creates a new PasswordEditingBridge and connects it with the delegate.
void LaunchPasswordEntryEditor(password_manager::PasswordStore* store,
                               const autofill::PasswordForm& password_form);

#endif  // CHROME_BROWSER_ANDROID_PASSWORD_UPDATE_DELEGATE_H_
