// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_delegate_helper.h"

#include "base/feature_list.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"

namespace password_manager {

LeakDetectionDelegateHelper::LeakDetectionDelegateHelper(
    scoped_refptr<PasswordStore> store,
    LeakTypeReply callback)
    : store_(std::move(store)), callback_(std::move(callback)) {
  DCHECK(store_);
}

LeakDetectionDelegateHelper::~LeakDetectionDelegateHelper() = default;

void LeakDetectionDelegateHelper::ProcessLeakedPassword(
    GURL url,
    base::string16 username,
    base::string16 password) {
  url_ = std::move(url);
  username_ = std::move(username);
  password_ = std::move(password);
  store_->GetLoginsByPassword(password_, this);
}

void LeakDetectionDelegateHelper::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  if (base::FeatureList::IsEnabled(features::kPasswordCheck)) {
    base::string16 canonicalized_username = CanonicalizeUsername(username_);
    for (const auto& form : results) {
      if (CanonicalizeUsername(form->username_value) ==
          canonicalized_username) {
        store_->AddCompromisedCredentials(
            {form->signon_realm, form->username_value, base::Time::Now(),
             CompromiseType::kLeaked});
      }
    }
  }

  IsSaved is_saved(
      std::any_of(results.begin(), results.end(), [this](const auto& form) {
        return form->origin == url_ && form->username_value == username_;
      }));

  IsReused is_reused(results.size() > (is_saved ? 1 : 0));
  std::move(callback_).Run(is_saved, is_reused, std::move(url_),
                           std::move(username_));
}

}  // namespace password_manager
