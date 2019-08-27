// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_delegate_helper.h"

#include "components/password_manager/core/browser/password_store.h"

namespace password_manager {

LeakDetectionDelegateHelper::LeakDetectionDelegateHelper(LeakTypeReply callback)
    : callback_(std::move(callback)) {}

LeakDetectionDelegateHelper::~LeakDetectionDelegateHelper() = default;

void LeakDetectionDelegateHelper::GetCredentialLeakType(
    PasswordStore* store,
    GURL url,
    base::string16 username,
    base::string16 password) {
  DCHECK(store);
  url_ = std::move(url);
  username_ = std::move(username);
  password_ = std::move(password);
  store->GetLoginsByPassword(password_, this);
}

void LeakDetectionDelegateHelper::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  bool is_saved = false;
  for (const auto& form : results) {
    if (form->origin == url_ && form->username_value == username_) {
      is_saved = true;
      break;
    }
  }
  bool is_reused = results.size() > (is_saved ? 1 : 0);
  CredentialLeakType leak_type =
      CreateLeakTypeFromBools(is_saved, is_reused, /*is_synced=*/true);
  std::move(callback_).Run(std::move(leak_type), std::move(url_),
                           std::move(username_));
}

}  // namespace password_manager
