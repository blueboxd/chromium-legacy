// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <sddl.h>  // For ConvertSidToStringSid()

#include "base/strings/utf_string_conversions.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/password_recovery_manager.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"
#include "chrome/credential_provider/test/test_credential.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

class GcpGaiaCredentialBaseTest : public GlsRunnerTestBase {};

TEST_F(GcpGaiaCredentialBaseTest, Advise) {
  // Create provider with credentials. This should Advise the credential.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  // Release ref count so the credential can be deleted by the call to
  // ReleaseProvider.
  cred.Release();

  // Release the provider. This should unadvise the credential.
  ASSERT_EQ(S_OK, ReleaseProvider());
}

TEST_F(GcpGaiaCredentialBaseTest, SetSelected) {
  // Create provider and credential only.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  // A credential that has not attempted to sign in a user yet should return
  // false for |auto_login|.
  BOOL auto_login;
  ASSERT_EQ(S_OK, cred->SetSelected(&auto_login));
  ASSERT_FALSE(auto_login);
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_NoInternet) {
  FakeInternetAvailabilityChecker internet_checker(
      FakeInternetAvailabilityChecker::kHicForceNo);

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  ASSERT_EQ(S_OK, StartLogonProcess(/*succeeds=*/false));
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Start) {
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Finish) {
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // Make sure a "foo" user was created.
  PSID sid;
  EXPECT_EQ(S_OK, fake_os_user_manager()->GetUserSID(
                      OSUserManager::GetLocalDomain().c_str(), kDefaultUsername,
                      &sid));
  ::LocalFree(sid);

  // New user should be created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
}

// This test emulates the scenario where SetDeselected is triggered by the
// Windows Login UI process after GetSerialization prior to invocation of
// ReportResult. Note: This currently happens only for OtherUser credential
// workflow.
TEST_F(GcpGaiaCredentialBaseTest,
       GetSerialization_SetDeselectedBeforeReportResult) {
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // Make sure a "foo" user was created.
  PSID sid;
  EXPECT_EQ(S_OK, fake_os_user_manager()->GetUserSID(
                      OSUserManager::GetLocalDomain().c_str(), kDefaultUsername,
                      &sid));

  // New user should be created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  // Finishing logon process should trigger credential changed and trigger
  // GetSerialization.
  ASSERT_EQ(S_OK, FinishLogonProcessWithCred(true, true, 0, cred));

  // Trigger SetDeselected prior to ReportResult is invoked.
  cred->SetDeselected();

  // Verify that the authentication results dictionary is not empty.
  ASSERT_FALSE(test->IsAuthenticationResultsEmpty());

  // Trigger ReportResult and verify that the authentication results are saved
  // into registry and ResetInternalState is triggered.
  ReportLogonProcessResult(cred);

  // Verify that the registry entry for the user was created.
  wchar_t gaia_id[256];
  ULONG length = base::size(gaia_id);
  wchar_t* sidstr = nullptr;
  ::ConvertSidToStringSid(sid, &sidstr);
  ::LocalFree(sid);

  HRESULT gaia_id_hr = GetUserProperty(sidstr, kUserId, gaia_id, &length);
  ASSERT_EQ(S_OK, gaia_id_hr);
  ASSERT_TRUE(gaia_id[0]);

  // Verify that the authentication results dictionary is now empty.
  ASSERT_TRUE(test->IsAuthenticationResultsEmpty());
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Abort) {
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));
  ASSERT_EQ(S_OK, test->SetDefaultExitCode(kUiecAbort));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Logon process should not signal credentials change or raise an error
  // message.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false, 0));
}

TEST_F(GcpGaiaCredentialBaseTest,
       GetSerialization_AssociateToMatchingAssociatedUser) {
  USES_CONVERSION;
  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR first_sid;
  base::string16 username(L"foo");
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      username, L"password", L"name", L"comment",
                      base::UTF8ToUTF16(kDefaultGaiaId), base::string16(),
                      &first_sid));
  ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // User should have been associated.
  EXPECT_EQ(test->GetFinalUsername(), username);
  // Email should be the same as the default one.
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // No new user should be created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_MultipleCalls) {
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  constexpr wchar_t kStartGlsEventName[] =
      L"GetSerialization_MultipleCalls_Wait";
  base::win::ScopedHandle start_event_handle(
      ::CreateEvent(nullptr, false, false, kStartGlsEventName));
  ASSERT_TRUE(start_event_handle.IsValid());
  ASSERT_EQ(S_OK, test->SetStartGlsEventName(kStartGlsEventName));
  base::WaitableEvent start_event(std::move(start_event_handle));

  ASSERT_EQ(S_OK, StartLogonProcess(/*succeeds=*/true));

  // Calling GetSerialization again while the credential is waiting for the
  // logon process should yield CPGSR_NO_CREDENTIAL_NOT_FINISHED as a
  // response.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  EXPECT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPSI_NONE, status_icon);
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Signal that the gls process can finish.
  start_event.Signal();

  ASSERT_EQ(S_OK, WaitForLogonProcess());
}

TEST_F(GcpGaiaCredentialBaseTest,
       GetSerialization_PasswordChangedForAssociatedUser) {
  USES_CONVERSION;

  // Create a fake user for which the windows password does not match the gaia
  // password supplied by the test gls process.
  CComBSTR sid;
  CComBSTR windows_password = L"password2";
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                L"foo", (BSTR)windows_password, L"Full Name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_OK, test->IsWindowsPasswordValidForStoredUser());

  // Check that the process has not finished yet.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Credentials should still be available.
  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_OK, test->IsWindowsPasswordValidForStoredUser());

  // Set an invalid password and try to get serialization again. Credentials
  // should still be valid but serialization is not complete.
  CComBSTR invalid_windows_password = L"a";
  cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD, invalid_windows_password);
  EXPECT_EQ(nullptr, status_text);
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Update the Windows password to be the real password created for the user.
  cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD, windows_password);
  // Sign in information should still be available.
  EXPECT_TRUE(test->GetFinalEmail().length());

  // Both Windows and Gaia credentials should be valid now
  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_FALSE, test->IsWindowsPasswordValidForStoredUser());

  // Finish logon successfully but with no credential changed event.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, false, 0));
}

TEST_F(GcpGaiaCredentialBaseTest,
       GetSerialization_ForgotPasswordForAssociatedUser) {
  USES_CONVERSION;

  // Create a fake user for which the windows password does not match the gaia
  // password supplied by the test gls process.
  CComBSTR sid;
  CComBSTR windows_password = L"password2";
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                L"foo", (BSTR)windows_password, L"Full Name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_OK, test->IsWindowsPasswordValidForStoredUser());

  // Check that the process has not finished yet.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Credentials should still be available.
  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_OK, test->IsWindowsPasswordValidForStoredUser());

  // Simulate a click on the "Forgot Password" link.
  cred->CommandLinkClicked(FID_FORGOT_PASSWORD_LINK);

  // Finish logon successfully but with no credential changed event.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, false, 0));
}

TEST_F(GcpGaiaCredentialBaseTest,
       GetSerialization_AlternateForgotPasswordAssociatedUser) {
  USES_CONVERSION;

  // Create a fake user for which the windows password does not match the gaia
  // password supplied by the test gls process.
  CComBSTR sid;
  CComBSTR windows_password = L"password2";
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                L"foo", (BSTR)windows_password, L"Full Name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_OK, test->IsWindowsPasswordValidForStoredUser());

  // Check that the process has not finished yet.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Credentials should still be available.
  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_OK, test->IsWindowsPasswordValidForStoredUser());

  // Simulate a click on the "Forgot Password" link.
  cred->CommandLinkClicked(FID_FORGOT_PASSWORD_LINK);

  // Go back to windows password entry.
  cred->CommandLinkClicked(FID_FORGOT_PASSWORD_LINK);

  // Set an invalid password and try to get serialization again. Credentials
  // should still be valid but serialization is not complete.
  CComBSTR invalid_windows_password = L"a";
  cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD, invalid_windows_password);
  EXPECT_EQ(nullptr, status_text);
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Update the Windows password to be the real password created for the user.
  cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD, windows_password);
  // Sign in information should still be available.
  EXPECT_TRUE(test->GetFinalEmail().length());

  // Both Windows and Gaia credentials should be valid now
  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_EQ(S_FALSE, test->IsWindowsPasswordValidForStoredUser());

  // Finish logon successfully but with no credential changed event.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, false, 0));
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Cancel) {
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  // This event is merely used to keep the gls running while it is cancelled
  // through SetDeselected().
  constexpr wchar_t kStartGlsEventName[] = L"GetSerialization_Cancel_Signal";
  base::win::ScopedHandle start_event_handle(
      ::CreateEvent(nullptr, false, false, kStartGlsEventName));
  ASSERT_TRUE(start_event_handle.IsValid());
  ASSERT_EQ(S_OK, test->SetStartGlsEventName(kStartGlsEventName));
  base::WaitableEvent start_event(std::move(start_event_handle));

  ASSERT_EQ(S_OK, StartLogonProcess(/*succeeds=*/true));

  // Deselect the credential provider so that it cancels the GLS process and
  // returns.
  ASSERT_EQ(S_OK, cred->SetDeselected());

  ASSERT_EQ(S_OK, WaitForLogonProcess());

  // Logon process should not signal credentials change or raise an error
  // message.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false, 0));
}

TEST_F(GcpGaiaCredentialBaseTest, FailedUserCreation) {
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  // Fail user creation.
  fake_os_user_manager()->SetShouldFailUserCreation(true);

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Logon process should fail with an internal error.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false, IDS_INTERNAL_ERROR_BASE));
}

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD) {
  USES_CONVERSION;
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  constexpr char email[] = "foo@imfl.info";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"foo_imfl"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, NewUserDisabledThroughUsageScenario) {
  USES_CONVERSION;
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  // Set the other user tile so that we can get the anonymous credential
  // that may try create a new user.
  fake_user_array()->SetAccountOptions(CPAO_EMPTY_LOCAL);

  SetUsageScenario(CPUS_UNLOCK_WORKSTATION);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Sign in should fail with an error stating that no new users can be created.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false,
                                     IDS_INVALID_UNLOCK_WORKSTATION_USER_BASE));
}

TEST_F(GcpGaiaCredentialBaseTest, NewUserDisabledThroughMdm) {
  USES_CONVERSION;
  // Enforce single user mode for MDM.
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 0));
  GoogleMdmEnrolledStatusForTesting force_success(true);

  // Create a fake user that is already associated so when the user tries to
  // sign on and create a new user, it fails.
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"foo_registered", L"password", L"name", L"comment",
                      L"gaia-id-registered", base::string16(), &sid));

  // Populate the associated users list. The created user's token handle
  // should be valid so that no reauth credential is created.
  fake_associated_user_validator()->StartRefreshingTokenHandleValidity();

  // Set the other user tile so that we can get the anonymous credential
  // that may try to sign in a user.
  fake_user_array()->SetAccountOptions(CPAO_EMPTY_LOCAL);

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Sign in should fail with an error stating that no new users can be created.
  ASSERT_EQ(S_OK,
            FinishLogonProcess(false, false, IDS_ADD_USER_DISALLOWED_BASE));
}

TEST_F(GcpGaiaCredentialBaseTest, InvalidUserUnlockedAfterSignin) {
  // Enforce token handle verification with user locking when the token handle
  // is not valid.
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  GoogleMdmEnrollmentStatusForTesting force_success(true);

  USES_CONVERSION;
  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR sid;
  base::string16 username(L"foo");
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                username, L"password", L"name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));
  ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  // Create with invalid token handle response.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  // User should have invalid token handle and be locked.
  EXPECT_FALSE(
      fake_associated_user_validator()->IsTokenHandleValidForUser(OLE2W(sid)));
  EXPECT_EQ(true,
            fake_associated_user_validator()->IsUserAccessBlockedForTesting(
                OLE2W(sid)));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // User should have been associated.
  EXPECT_EQ(test->GetFinalUsername(), username);
  // Email should be the same as the default one.
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // Now finish the logon, this should unlock the user.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));

  EXPECT_EQ(false,
            fake_associated_user_validator()->IsUserAccessBlockedForTesting(
                OLE2W(sid)));

  // No new user should be created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
}

TEST_F(GcpGaiaCredentialBaseTest, DenySigninBlockedDuringSignin) {
  USES_CONVERSION;

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  GoogleMdmEnrolledStatusForTesting force_success(true);

  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR first_sid;
  base::string16 username(L"foo");
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      username, L"password", L"name", L"comment",
                      base::UTF8ToUTF16(kDefaultGaiaId), base::string16(),
                      &first_sid));
  ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  // Create with valid token handle response and sign in the anonymous
  // credential with the user that should still be valid.
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Change token response to an invalid one.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(AssociatedUserValidator::kTokenInfoUrl),
      FakeWinHttpUrlFetcher::Headers(), "{}");

  // Force refresh of all token handles on the next query.
  fake_associated_user_validator()->ForceRefreshTokenHandlesForTesting();

  // Signin process has already started. User should not be locked even if their
  // token handle is invalid.
  EXPECT_FALSE(fake_associated_user_validator()
                   ->DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON));
  EXPECT_FALSE(fake_associated_user_validator()->IsUserAccessBlockedForTesting(
      OLE2W(first_sid)));

  // Now finish the logon.
  ASSERT_EQ(S_OK, FinishLogonProcessWithCred(true, true, 0, cred));

  // User should have been associated.
  EXPECT_EQ(test->GetFinalUsername(), username);
  // Email should be the same as the default one.
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // Result has not been reported yet, user signin should still not be denied.
  EXPECT_FALSE(fake_associated_user_validator()
                   ->DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON));
  EXPECT_FALSE(fake_associated_user_validator()->IsUserAccessBlockedForTesting(
      OLE2W(first_sid)));

  ReportLogonProcessResult(cred);

  // Now signin can be denied for the user if their token handle is invalid.
  EXPECT_TRUE(fake_associated_user_validator()
                  ->DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON));
  EXPECT_TRUE(fake_associated_user_validator()->IsUserAccessBlockedForTesting(
      OLE2W(first_sid)));

  // No new user should be created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
}

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD_Gmail) {
  USES_CONVERSION;

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  constexpr char email[] = "bar@gmail.com";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"bar"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD_Googlemail) {
  USES_CONVERSION;

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  constexpr char email[] = "toto@googlemail.com";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"toto"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, InvalidUsernameCharacters) {
  USES_CONVERSION;
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  constexpr char email[] = "a\\[]:|<>+=;?*z@gmail.com";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"a____________z"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, EmailTooLong) {
  USES_CONVERSION;

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  constexpr char email[] = "areallylongemailadressdude@gmail.com";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"areallylongemailadre"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, EmailTooLong2) {
  USES_CONVERSION;
  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  constexpr char email[] = "foo@areallylongdomaindude.com";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"foo_areallylongdomai"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, EmailIsNoAt) {
  USES_CONVERSION;
  constexpr char email[] = "foo";

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"foo_gmail"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, EmailIsAtCom) {
  USES_CONVERSION;

  constexpr char email[] = "@com";

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"_com"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, EmailIsAtDotCom) {
  USES_CONVERSION;

  constexpr char email[] = "@.com";

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"_.com"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

// Tests various sign in scenarios with consumer and non-consumer domains.
// Parameters are:
// 1. Is mdm enrollment enabled.
// 2. The mdm_aca reg key setting:
//    - 0: Set reg key to 0.
//    - 1: Set reg key to 1.
//    - 2: Don't set reg key.
// 3. Whether the mdm_aca reg key is set to 1 or 0.
// 4. Whether an existing associated user is already present.
// 5. Whether the user being created (or existing) uses a consumer account.
class GcpGaiaCredentialBaseConsumerEmailTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<std::tuple<bool, int, bool, bool>> {
};

TEST_P(GcpGaiaCredentialBaseConsumerEmailTest, ConsumerEmailSignin) {
  USES_CONVERSION;
  const bool mdm_enabled = std::get<0>(GetParam());
  const int mdm_consumer_accounts_reg_key_setting = std::get<1>(GetParam());
  const bool user_created = std::get<2>(GetParam());
  const bool user_is_consumer = std::get<3>(GetParam());

  FakeAssociatedUserValidator validator;
  FakeInternetAvailabilityChecker internet_checker;
  GoogleMdmEnrollmentStatusForTesting force_success(true);

  if (mdm_enabled)
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));

  const bool mdm_consumer_accounts_reg_key_set =
      mdm_consumer_accounts_reg_key_setting >= 0 &&
      mdm_consumer_accounts_reg_key_setting < 2;
  if (mdm_consumer_accounts_reg_key_set) {
    ASSERT_EQ(S_OK,
              SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts,
                                      mdm_consumer_accounts_reg_key_setting));
  }

  std::string user_email = user_is_consumer ? kDefaultEmail : "foo@imfl.info";

  CComBSTR sid;
  base::string16 username(user_is_consumer ? L"foo" : L"foo_imfl");

  // Create a fake user that has the same gaia id as the test gaia id.
  if (user_created) {
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        username, L"password", L"name", L"comment",
                        base::UTF8ToUTF16(kDefaultGaiaId),
                        base::UTF8ToUTF16(user_email), &sid));
    ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());
  }

  // Create provider and start logon.
  CComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  CComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.QueryInterface(&test));

  test->SetGlsEmailAddress(user_email);

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  bool should_signin_succeed = !mdm_enabled ||
                               (mdm_consumer_accounts_reg_key_set &&
                                mdm_consumer_accounts_reg_key_setting) ||
                               !user_is_consumer;

  // Sign in success.
  if (should_signin_succeed) {
    // User should have been associated.
    EXPECT_EQ(test->GetFinalUsername(), username);
    // Email should be the same as the default one.
    EXPECT_EQ(test->GetFinalEmail(), user_email);

    ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));
  } else {
    // Error message concerning invalid domain is sent.
    ASSERT_EQ(S_OK,
              FinishLogonProcess(false, false, IDS_INVALID_EMAIL_DOMAIN_BASE));
  }

  if (user_created) {
    // No new user should be created.
    EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
  } else {
    // New user created only if their domain is valid for the sign in.
    EXPECT_EQ(1ul + (should_signin_succeed ? 1ul : 0ul),
              fake_os_user_manager()->GetUserCount());
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         GcpGaiaCredentialBaseConsumerEmailTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Values(0, 1, 2),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

// Test password recovery system for various failure success cases.
// Parameters are:
// 1. int - The expected result of the initial public key retrieval for storing
//          the password. Values are 0 - success, 1 - failure, 2 - timeout.
// 2. int - The expected result of the initial public private retrieval for
//          decrypting the password. Values are 0 - success, 1 - failure,
//          2 - timeout.
// 3. int - The expected result of the initial public private retrieval for
//          decrypting the password. Values are 0 - success, 1 - failure,
//          2 - timeout.
class GcpGaiaCredentialBasePasswordRecoveryTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<std::tuple<int, int, int>> {};

TEST_P(GcpGaiaCredentialBasePasswordRecoveryTest, PasswordRecovery) {
  // Enable standard escrow service features in non-Chrome builds so that
  // the escrow service code can be tested by the build machines.
#if !defined(GOOGLE_CHROME_BUILD)
  GoogleMdmEscrowServiceEnablerForTesting escrow_service_enabler(true);
#endif
  USES_CONVERSION;

  int generate_public_key_result = std::get<0>(GetParam());
  int get_private_key_result = std::get<1>(GetParam());
  int generate_public_key_again_result = std::get<2>(GetParam());

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmEscrowServiceServerUrl,
                                          L"https://escrow.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));

  GoogleMdmEnrolledStatusForTesting force_success(true);

  // Create a fake user associated to a gaia id.
  CComBSTR sid;
  constexpr wchar_t kOldPassword[] = L"password";
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                kDefaultUsername, kOldPassword, L"Full Name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));

  // Change token response to an invalid one.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);

  // Make a dummy response for successful public key generation and private key
  // retrieval.
  std::string generate_success_response =
      fake_password_recovery_manager()->MakeGenerateKeyPairResponseForTesting(
          "public_key", "resource_id");

  std::string get_key_success_response =
      fake_password_recovery_manager()->MakeGetPrivateKeyResponseForTesting(
          "private_key");

  // Make timeout events for the various escrow service requests if needed.
  std::unique_ptr<base::WaitableEvent> get_key_event;
  std::unique_ptr<base::WaitableEvent> generate_key_event;

  if (generate_public_key_result == 2)
    get_key_event.reset(new base::WaitableEvent());

  if (get_private_key_result == 2)
    generate_key_event.reset(new base::WaitableEvent());

  if (get_key_event || generate_key_event) {
    fake_password_recovery_manager()->SetRequestTimeoutForTesting(
        base::TimeDelta::FromMilliseconds(50));
  }

  fake_http_url_fetcher_factory()->SetFakeResponse(
      fake_password_recovery_manager()->GetEscrowServiceGenerateKeyPairUrl(),
      FakeWinHttpUrlFetcher::Headers(),
      generate_public_key_result != 1 ? generate_success_response : "{}",
      generate_key_event ? generate_key_event->handle() : INVALID_HANDLE_VALUE);

  fake_http_url_fetcher_factory()->SetFakeResponse(
      fake_password_recovery_manager()->GetEscrowServiceGetPrivateKeyUrl(),
      FakeWinHttpUrlFetcher::Headers(),
      get_private_key_result != 1 ? get_key_success_response : "{}",
      get_key_event ? get_key_event->handle() : INVALID_HANDLE_VALUE);

  bool should_store_succeed = generate_public_key_result == 0;
  bool should_recover_succeed = get_private_key_result == 0;

  // Sign on once to store the password in the LSA
  {
    // Create provider and start logon.
    CComPtr<ICredentialProviderCredential> cred;

    ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

    ASSERT_EQ(S_OK, StartLogonProcessAndWait());

    // Finish logon successfully to propagate password recovery information to
    // LSA.
    ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));

    ASSERT_EQ(S_OK, ReleaseProvider());
  }

  // If there was a timeout for the generation of the public key, signal it now
  // so that the request thread can complete. Also delete the event in case it
  // needs to be used again on the sign in after the password was retrieved.
  if (generate_key_event) {
    generate_key_event->Signal();
    generate_key_event.reset();
  }

  if (generate_public_key_again_result == 2)
    generate_key_event.reset(new base::WaitableEvent());

  if (generate_key_event) {
    fake_password_recovery_manager()->SetRequestTimeoutForTesting(
        base::TimeDelta::FromMilliseconds(50));
  }

  fake_http_url_fetcher_factory()->SetFakeResponse(
      fake_password_recovery_manager()->GetEscrowServiceGenerateKeyPairUrl(),
      FakeWinHttpUrlFetcher::Headers(),
      generate_public_key_again_result != 1 ? generate_success_response : "{}",
      generate_key_event ? generate_key_event->handle() : INVALID_HANDLE_VALUE);

  constexpr char kNewPassword[] = "password2";

  // Sign in a second time with a different password and see if it is updated
  // automatically.
  {
    // Create provider and start logon.
    CComPtr<ICredentialProviderCredential> cred;

    ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

    CComPtr<ITestCredential> test;
    ASSERT_EQ(S_OK, cred.QueryInterface(&test));

    // Send back a different gaia password to force a password update.
    ASSERT_EQ(S_OK, test->SetGlsGaiaPassword(kNewPassword));

    // Don't send a forced e-mail. It will be sent from the user that was
    // updated during the last sign in.
    ASSERT_EQ(S_OK, test->SetGlsEmailAddress(std::string()));

    ASSERT_EQ(S_OK, StartLogonProcessAndWait());

    CComPtr<ITestCredentialProvider> test_provider;
    ASSERT_EQ(S_OK, created_provider().QueryInterface(&test_provider));

    // If either password storage or recovery failed then the user will need to
    // enter their old Windows password.
    if (!should_store_succeed || !should_recover_succeed) {
      // Logon should not complete but there is no error message.
      EXPECT_EQ(test_provider->credentials_changed_fired(), false);

      // Set the correct old password so that the user can sign in.
      ASSERT_EQ(S_OK,
                cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD, kOldPassword));

      // Finish logon successfully now which should update the password.
      ASSERT_EQ(S_OK, FinishLogonProcess(true, false, 0));
    } else {
      // Make sure the new password is sent to the provider.
      EXPECT_STREQ(A2OLE(kNewPassword), OLE2CW(test_provider->password()));

      // Finish logon successfully but with no credential changed event.
      ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));
    }

    // Make sure the user has the new password internally.
    EXPECT_EQ(S_OK, fake_os_user_manager()->IsWindowsPasswordValid(
                        OSUserManager::GetLocalDomain().c_str(),
                        kDefaultUsername, A2OLE(kNewPassword)));

    ASSERT_EQ(S_OK, ReleaseProvider());
  }

  // Complete the private key retrieval request if it was waiting.
  if (get_key_event)
    get_key_event->Signal();

  // If generate of the second public key failed, the next sign in would
  // need to re-enter their password
  if (generate_public_key_again_result != 0) {
    constexpr char kNewPassword2[] = "password3";
    // Create provider and start logon.
    CComPtr<ICredentialProviderCredential> cred;

    ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

    CComPtr<ITestCredential> test;
    ASSERT_EQ(S_OK, cred.QueryInterface(&test));

    // Send back a different gaia password to force a password update.
    ASSERT_EQ(S_OK, test->SetGlsGaiaPassword(kNewPassword2));

    // Don't send a forced e-mail. It will be sent from the user that was
    // updated during the last sign in.
    ASSERT_EQ(S_OK, test->SetGlsEmailAddress(std::string()));

    ASSERT_EQ(S_OK, StartLogonProcessAndWait());

    CComPtr<ITestCredentialProvider> test_provider;
    ASSERT_EQ(S_OK, created_provider().QueryInterface(&test_provider));

    // Logon should not complete but there is no error message.
    EXPECT_EQ(test_provider->credentials_changed_fired(), false);

    // Set the correct old password so that the user can sign in.
    ASSERT_EQ(S_OK,
              cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD,
                                   base::UTF8ToUTF16(kNewPassword).c_str()));

    // Finish logon successfully now which should update the password.
    ASSERT_EQ(S_OK, FinishLogonProcess(true, false, 0));

    // Make sure the user has the new password internally.
    EXPECT_EQ(S_OK, fake_os_user_manager()->IsWindowsPasswordValid(
                        OSUserManager::GetLocalDomain().c_str(),
                        kDefaultUsername, A2OLE(kNewPassword2)));

    ASSERT_EQ(S_OK, ReleaseProvider());
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         GcpGaiaCredentialBasePasswordRecoveryTest,
                         ::testing::Combine(::testing::Values(0, 1, 2),
                                            ::testing::Values(0, 1, 2),
                                            ::testing::Values(0, 1, 2)));

// Test password recovery system being disabled by registry settings.
// Parameter is a pointer to an escrow service url. Can be empty or nullptr.
class GcpGaiaCredentialBasePasswordRecoveryDisablingTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<const wchar_t*> {};

TEST_P(GcpGaiaCredentialBasePasswordRecoveryDisablingTest,
       PasswordRecovery_Disabled) {
  // Enable standard escrow service features in non-Chrome builds so that
  // the escrow service code can be tested by the build machines.
#if !defined(GOOGLE_CHROME_BUILD)
  GoogleMdmEscrowServiceEnablerForTesting escrow_service_enabler(true);
#endif
  USES_CONVERSION;
  const wchar_t* escrow_service_url = GetParam();

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  if (escrow_service_url) {
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS,
              key.Create(HKEY_LOCAL_MACHINE, kGcpRootKeyName, KEY_WRITE));
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kRegMdmEscrowServiceServerUrl,
                                            escrow_service_url));
  }

  GoogleMdmEnrolledStatusForTesting force_success(true);

  // Create a fake user associated to a gaia id.
  CComBSTR sid;
  constexpr wchar_t kOldPassword[] = L"password";
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                kDefaultUsername, kOldPassword, L"Full Name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));

  // Change token response to an invalid one.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);

  // Make a dummy response for successful public key generation and private key
  // retrieval.
  std::string generate_success_response =
      fake_password_recovery_manager()->MakeGenerateKeyPairResponseForTesting(
          "public_key", "resource_id");

  std::string get_key_success_response =
      fake_password_recovery_manager()->MakeGetPrivateKeyResponseForTesting(
          "private_key");

  fake_http_url_fetcher_factory()->SetFakeResponse(
      fake_password_recovery_manager()->GetEscrowServiceGenerateKeyPairUrl(),
      FakeWinHttpUrlFetcher::Headers(), generate_success_response);

  fake_http_url_fetcher_factory()->SetFakeResponse(
      fake_password_recovery_manager()->GetEscrowServiceGetPrivateKeyUrl(),
      FakeWinHttpUrlFetcher::Headers(), get_key_success_response);

  // Sign on once to store the password in the LSA
  {
    // Create provider and start logon.
    CComPtr<ICredentialProviderCredential> cred;

    ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

    ASSERT_EQ(S_OK, StartLogonProcessAndWait());

    // Finish logon successfully to propagate password recovery information to
    // LSA.
    ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));

    ASSERT_EQ(S_OK, ReleaseProvider());
  }

  // Sign in a second time with a different password and see if it is updated
  // automatically.
  {
    constexpr char kNewPassword[] = "password2";

    // Create provider and start logon.
    CComPtr<ICredentialProviderCredential> cred;

    ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

    CComPtr<ITestCredential> test;
    ASSERT_EQ(S_OK, cred.QueryInterface(&test));

    // Send back a different gaia password to force a password update.
    ASSERT_EQ(S_OK, test->SetGlsGaiaPassword(kNewPassword));

    // Don't send a forced e-mail. It will be sent from the user that was
    // updated during the last sign in.
    ASSERT_EQ(S_OK, test->SetGlsEmailAddress(std::string()));

    ASSERT_EQ(S_OK, StartLogonProcessAndWait());

    CComPtr<ITestCredentialProvider> test_provider;
    ASSERT_EQ(S_OK, created_provider().QueryInterface(&test_provider));

    // Null or empty escrow service url will disable password
    // recovery and force the user to enter their password.
    if (!escrow_service_url || escrow_service_url[0] == '\0') {
      // Logon should not complete but there is no error message.
      EXPECT_EQ(test_provider->credentials_changed_fired(), false);

      // Set the correct old password so that the user can sign in.
      ASSERT_EQ(S_OK,
                cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD, kOldPassword));

      // Finish logon successfully now which should update the password.
      ASSERT_EQ(S_OK, FinishLogonProcess(true, false, 0));
    } else {
      // Make sure the new password is sent to the provider.
      EXPECT_STREQ(A2OLE(kNewPassword), OLE2CW(test_provider->password()));

      // Finish logon successfully but with no credential changed event.
      ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));
    }

    // Make sure the user has the new password internally.
    EXPECT_EQ(S_OK, fake_os_user_manager()->IsWindowsPasswordValid(
                        OSUserManager::GetLocalDomain().c_str(),
                        kDefaultUsername, A2OLE(kNewPassword)));

    ASSERT_EQ(S_OK, ReleaseProvider());
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         GcpGaiaCredentialBasePasswordRecoveryDisablingTest,
                         ::testing::Values(nullptr,
                                           L"",
                                           L"https://escrowservice.com"));
}  // namespace testing
}  // namespace credential_provider
