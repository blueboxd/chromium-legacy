// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_MOCK_ENROLLMENT_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_MOCK_ENROLLMENT_SCREEN_H_

#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/policy/enrollment_status.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockEnrollmentScreen : public EnrollmentScreen {
 public:
  MockEnrollmentScreen(EnrollmentScreenView* view,
                       const ScreenExitCallback& exit_callback);
  ~MockEnrollmentScreen() override;

  MOCK_METHOD0(ShowImpl, void());
  MOCK_METHOD0(HideImpl, void());

  void ExitScreen(Result result);
};

class MockEnrollmentScreenView : public EnrollmentScreenView {
 public:
  MockEnrollmentScreenView();
  virtual ~MockEnrollmentScreenView();

  MOCK_METHOD2(SetEnrollmentConfig,
               void(Controller*, const policy::EnrollmentConfig& config));
  MOCK_METHOD2(SetEnterpriseDomainAndDeviceType,
               void(const std::string& domain,
                    const base::string16& device_type));
  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD0(ShowSigninScreen, void());
  MOCK_METHOD1(ShowLicenseTypeSelectionScreen,
               void(const base::DictionaryValue&));
  MOCK_METHOD4(ShowActiveDirectoryScreen,
               void(const std::string& domain_join_config,
                    const std::string& machine_name,
                    const std::string& username,
                    authpolicy::ErrorType error));
  MOCK_METHOD2(ShowAttributePromptScreen,
               void(const std::string& asset_id, const std::string& location));
  MOCK_METHOD0(ShowEnrollmentSuccessScreen, void());
  MOCK_METHOD0(ShowEnrollmentSpinnerScreen, void());
  MOCK_METHOD1(ShowAuthError, void(const GoogleServiceAuthError&));
  MOCK_METHOD1(ShowOtherError, void(EnterpriseEnrollmentHelper::OtherError));
  MOCK_METHOD1(ShowEnrollmentStatus, void(policy::EnrollmentStatus status));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_MOCK_ENROLLMENT_SCREEN_H_
