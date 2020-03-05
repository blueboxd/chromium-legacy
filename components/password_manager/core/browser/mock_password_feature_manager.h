// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FEATURE_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FEATURE_MANAGER_H_

#include "components/password_manager/core/browser/password_feature_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordFeatureManager : public PasswordFeatureManager {
 public:
  MockPasswordFeatureManager();
  ~MockPasswordFeatureManager() override;

  MOCK_CONST_METHOD0(IsGenerationEnabled, bool());

  MOCK_CONST_METHOD0(IsOptedInForAccountStorage, bool());
  MOCK_CONST_METHOD0(ShouldShowAccountStorageOptIn, bool());
  MOCK_METHOD1(SetAccountStorageOptIn, void(bool));

  MOCK_CONST_METHOD0(ShouldShowPasswordStorePicker, bool());

  MOCK_METHOD1(SetDefaultPasswordStore,
               void(const autofill::PasswordForm::Store& store));
  MOCK_CONST_METHOD0(GetDefaultPasswordStore, autofill::PasswordForm::Store());
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_FEATURE_MANAGER_H_
