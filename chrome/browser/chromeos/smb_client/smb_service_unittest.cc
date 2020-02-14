// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_service.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/fake_registry.h"
#include "chrome/browser/chromeos/file_system_provider/icon_set.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/smb_client/smb_file_system_id.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_smb_provider_client.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Ne;
using ::testing::WithArg;
using ::testing::WithArgs;

namespace chromeos {
namespace smb_client {

namespace {

const file_system_provider::ProviderId kProviderId =
    file_system_provider::ProviderId::CreateFromNativeId("smb");
constexpr char kTestUser[] = "foobar";
constexpr char kTestPassword[] = "my_secret_password";
constexpr char kTestDomain[] = "EXAMPLE.COM";
constexpr char kSharePath[] = "\\\\server\\foobar";
constexpr char kShareUrl[] = "smb://server/foobar";
constexpr char kDisplayName[] = "My Share";

constexpr char kTestADUser[] = "ad-test-user";
constexpr char kTestADDomain[] = "foorbar.corp";
constexpr char kTestADGuid[] = "ad-user-guid";

void SaveMountResult(SmbMountResult* out, SmbMountResult result) {
  *out = result;
}

class MockSmbProviderClient : public chromeos::FakeSmbProviderClient {
 public:
  MockSmbProviderClient()
      : FakeSmbProviderClient(true /* should_run_synchronously */) {}

  MOCK_METHOD(void,
              Mount,
              (const base::FilePath& share_path,
               const MountOptions& options,
               base::ScopedFD password_fd,
               SmbProviderClient::MountCallback callback),
              (override));
  MOCK_METHOD(void,
              SetupKerberos,
              (const std::string& account_id,
               SmbProviderClient::SetupKerberosCallback callback),
              (override));
};

// Gets a password from |password_fd|. The data has to be in the format of
// "{password_length}{password}".
std::string GetPassword(const base::ScopedFD& password_fd) {
  size_t password_length = 0;

  // Read sizeof(password_length) bytes from the file to get the length.
  EXPECT_TRUE(base::ReadFromFD(password_fd.get(),
                               reinterpret_cast<char*>(&password_length),
                               sizeof(password_length)));

  // Read the password into the buffer.
  std::string password(password_length, 'a');
  EXPECT_TRUE(
      base::ReadFromFD(password_fd.get(), &password[0], password_length));
  return password;
}

}  // namespace

class SmbServiceTest : public testing::Test {
 protected:
  SmbServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD) {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(profile_manager_->SetUp());

    std::unique_ptr<FakeChromeUserManager> user_manager_temp =
        std::make_unique<FakeChromeUserManager>();

    profile_ = profile_manager_->CreateTestingProfile("test-user@example.com");
    user_manager_temp->AddUser(
        AccountId::FromUserEmail(profile_->GetProfileUserName()));

    ad_user_email_ = base::StrCat({kTestADUser, "@", kTestADDomain});
    ad_profile_ = profile_manager_->CreateTestingProfile(ad_user_email_);
    user_manager_temp->AddUserWithAffiliationAndTypeAndProfile(
        AccountId::AdFromUserEmailObjGuid(ad_profile_->GetProfileUserName(),
                                          kTestADGuid),
        false, user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY, ad_profile_);

    // Run pending async tasks resulting from profile construction to ensure
    // these are complete before the test begins.
    base::RunLoop().RunUntilIdle();

    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager_temp));

    mock_client_ = new MockSmbProviderClient;
    // The mock needs to be marked as leaked because ownership is passed to
    // DBusThreadManager.
    testing::Mock::AllowLeak(mock_client_);
    chromeos::DBusThreadManager::GetSetterForTesting()->SetSmbProviderClient(
        base::WrapUnique(mock_client_));

    mount_options_.display_name = kDisplayName;
  }

  ~SmbServiceTest() override {}

  void CreateFspRegistry(TestingProfile* profile) {
    // Create FSP service.
    registry_ = new file_system_provider::FakeRegistry;
    file_system_provider::Service::Get(profile)->SetRegistryForTesting(
        base::WrapUnique(registry_));
  }

  void CreateService(TestingProfile* profile) {
    SmbService::DisableShareDiscoveryForTesting();

    // Create smb service.
    smb_service_ = std::make_unique<SmbService>(
        profile, std::make_unique<base::SimpleTestTickClock>());
  }

  void ExpectInvalidUrl(const std::string& url) {
    SmbMountResult result = SmbMountResult::kSuccess;
    smb_service_->Mount({} /* options */, base::FilePath(url),
                        "" /* username */, "" /* password */,
                        false /* use_chromad_kerberos */,
                        false /* should_open_file_manager_after_mount */,
                        false /* save_credentials */,
                        base::BindOnce(&SaveMountResult, &result));
    EXPECT_EQ(result, SmbMountResult::kInvalidUrl);
  }

  void ExpectInvalidSsoUrl(const std::string& url) {
    SmbMountResult result = SmbMountResult::kSuccess;
    smb_service_->Mount({} /* options */, base::FilePath(url),
                        "" /* username */, "" /* password */,
                        true /* use_chromad_kerberos */,
                        false /* should_open_file_manager_after_mount */,
                        false /* save_credentials */,
                        base::BindOnce(&SaveMountResult, &result));
    EXPECT_EQ(result, SmbMountResult::kInvalidSsoUrl);
  }

  void WaitForSetupComplete() {
    {
      base::RunLoop run_loop;
      smb_service_->OnSetupCompleteForTesting(run_loop.QuitClosure());
      run_loop.Run();
    }
    {
      // Share gathering needs to complete at least once before a share can be
      // mounted.
      base::RunLoop run_loop;
      smb_service_->GatherSharesInNetwork(
          base::DoNothing(),
          base::BindLambdaForTesting(
              [&run_loop](const std::vector<SmbUrl>& shares_gathered,
                          bool done) {
                if (done) {
                  run_loop.Quit();
                }
              }));
      run_loop.Run();
    }
  }

  content::BrowserTaskEnvironment
      task_environment_;  // Included so tests magically don't crash.
  TestingProfile* profile_ = nullptr;     // Not owned.
  std::string ad_user_email_;
  TestingProfile* ad_profile_ = nullptr;  // Not owned.
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  MockSmbProviderClient* mock_client_;  // Owned by DBusThreadManager.
  std::unique_ptr<SmbService> smb_service_;

  // Owned by file_system_provider::Service.
  file_system_provider::FakeRegistry* registry_;

  chromeos::file_system_provider::MountOptions mount_options_;
};

TEST_F(SmbServiceTest, InvalidUrls) {
  CreateService(profile_);

  ExpectInvalidUrl("");
  ExpectInvalidUrl("foo");
  ExpectInvalidUrl("\\foo");
  ExpectInvalidUrl("\\\\foo");
  ExpectInvalidUrl("\\\\foo\\");
  ExpectInvalidUrl("file://foo/bar");
  ExpectInvalidUrl("smb://foo");
  ExpectInvalidUrl("smb://user@password:foo");
  ExpectInvalidUrl("smb:\\\\foo\\bar");
  ExpectInvalidUrl("//foo/bar");
}

TEST_F(SmbServiceTest, InvalidSsoUrls) {
  CreateService(profile_);

  ExpectInvalidSsoUrl("\\\\192.168.1.1\\foo");
  ExpectInvalidSsoUrl("\\\\[0:0:0:0:0:0:0:1]\\foo");
  ExpectInvalidSsoUrl("\\\\[::1]\\foo");
  ExpectInvalidSsoUrl("smb://192.168.1.1/foo");
  ExpectInvalidSsoUrl("smb://[0:0:0:0:0:0:0:1]/foo");
  ExpectInvalidSsoUrl("smb://[::1]/foo");
}

TEST_F(SmbServiceTest, Mount) {
  CreateFspRegistry(profile_);
  CreateService(profile_);
  WaitForSetupComplete();

  base::RunLoop run_loop;
  EXPECT_CALL(
      *mock_client_,
      Mount(
          base::FilePath(kShareUrl),
          AllOf(Field(&SmbProviderClient::MountOptions::username, kTestUser),
                Field(&SmbProviderClient::MountOptions::workgroup, ""),
                Field(&SmbProviderClient::MountOptions::ntlm_enabled, true),
                Field(&SmbProviderClient::MountOptions::skip_connect, false),
                Field(&SmbProviderClient::MountOptions::save_password, false)),
          _, _))
      .WillOnce(
          WithArgs<2, 3>(Invoke([](base::ScopedFD password_fd,
                                   SmbProviderClient::MountCallback callback) {
            EXPECT_EQ(kTestPassword, GetPassword(password_fd));
            std::move(callback).Run(smbprovider::ErrorType::ERROR_OK, 7);
          })));

  smb_service_->Mount(
      mount_options_, base::FilePath(kSharePath), kTestUser, kTestPassword,
      false /* use_chromad_kerberos */,
      false /* should_open_file_manager_after_mount */,
      false /* save_credentials */,
      base::BindLambdaForTesting([&run_loop](SmbMountResult result) {
        EXPECT_EQ(SmbMountResult::kSuccess, result);
        run_loop.Quit();
      }));
  run_loop.Run();

  // If |save_credentials| is false, then the username should not be saved in
  // the file system id.
  const std::string file_system_id =
      registry_->file_system_info()->file_system_id();
  EXPECT_FALSE(IsKerberosChromadFileSystemId(file_system_id));
  EXPECT_FALSE(GetUserFromFileSystemId(file_system_id));

  // Because the mock is potentially leaked, expectations needs to be manually
  // verified.
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(mock_client_));
}

TEST_F(SmbServiceTest, MountSaveCredentials) {
  CreateFspRegistry(profile_);
  CreateService(profile_);
  WaitForSetupComplete();

  base::RunLoop run_loop;
  EXPECT_CALL(
      *mock_client_,
      Mount(
          base::FilePath(kShareUrl),
          AllOf(Field(&SmbProviderClient::MountOptions::username, kTestUser),
                Field(&SmbProviderClient::MountOptions::workgroup, ""),
                Field(&SmbProviderClient::MountOptions::ntlm_enabled, true),
                Field(&SmbProviderClient::MountOptions::skip_connect, false),
                Field(&SmbProviderClient::MountOptions::save_password, true),
                Field(&SmbProviderClient::MountOptions::account_hash, Ne(""))),
          _, _))
      .WillOnce(
          WithArgs<2, 3>(Invoke([](base::ScopedFD password_fd,
                                   SmbProviderClient::MountCallback callback) {
            EXPECT_EQ(kTestPassword, GetPassword(password_fd));
            std::move(callback).Run(smbprovider::ErrorType::ERROR_OK, 7);
          })));

  smb_service_->Mount(
      mount_options_, base::FilePath(kSharePath), kTestUser, kTestPassword,
      false /* use_chromad_kerberos */,
      false /* should_open_file_manager_after_mount */,
      true /* save_credentials */,
      base::BindLambdaForTesting([&run_loop](SmbMountResult result) {
        EXPECT_EQ(SmbMountResult::kSuccess, result);
        run_loop.Quit();
      }));
  run_loop.Run();

  const std::string file_system_id =
      registry_->file_system_info()->file_system_id();
  EXPECT_FALSE(IsKerberosChromadFileSystemId(file_system_id));
  base::Optional<std::string> saved_user =
      GetUserFromFileSystemId(file_system_id);
  ASSERT_TRUE(saved_user);
  EXPECT_EQ(*saved_user, kTestUser);

  // Because the mock is potentially leaked, expectations needs to be manually
  // verified.
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(mock_client_));
}

TEST_F(SmbServiceTest, Remount) {
  file_system_provider::MountOptions mount_options(
      CreateFileSystemId(base::FilePath(kSharePath),
                         false /* is_kerberos_chromad */),
      "Foo");
  ProvidedFileSystemInfo file_system_info(
      kProviderId, mount_options, base::FilePath(kSharePath),
      false /* configurable */, false /* watchable */,
      extensions::SOURCE_NETWORK, chromeos::file_system_provider::IconSet());
  CreateFspRegistry(profile_);
  registry_->RememberFileSystem(file_system_info, {});

  base::RunLoop run_loop;
  EXPECT_CALL(
      *mock_client_,
      Mount(base::FilePath(kShareUrl),
            AllOf(Field(&SmbProviderClient::MountOptions::skip_connect, true),
                  Field(&SmbProviderClient::MountOptions::restore_password,
                        false)),
            _, _))
      .WillOnce(WithArgs<2, 3>(
          Invoke([&run_loop](base::ScopedFD password_fd,
                             SmbProviderClient::MountCallback callback) {
            // Should have a valid password_fd containing an empty password.
            EXPECT_EQ("", GetPassword(password_fd));
            std::move(callback).Run(smbprovider::ErrorType::ERROR_OK, 7);
            run_loop.Quit();
          })));

  CreateService(profile_);
  run_loop.Run();

  // Because the mock is potentially leaked, expectations needs to be manually
  // verified.
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(mock_client_));
}

TEST_F(SmbServiceTest, Remount_ActiveDirectory) {
  file_system_provider::MountOptions mount_options(
      CreateFileSystemId(base::FilePath(kSharePath),
                         true /* is_kerberos_chromad */),
      "Foo");
  ProvidedFileSystemInfo file_system_info(
      kProviderId, mount_options, base::FilePath(kSharePath),
      false /* configurable */, false /* watchable */,
      extensions::SOURCE_NETWORK, chromeos::file_system_provider::IconSet());
  CreateFspRegistry(ad_profile_);
  registry_->RememberFileSystem(file_system_info, {});

  base::RunLoop run_loop;

  EXPECT_CALL(*mock_client_, SetupKerberos(kTestADGuid, _))
      .WillOnce(WithArg<1>(
          Invoke([](SmbProviderClient::SetupKerberosCallback callback) {
            base::ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), true));
          })));
  EXPECT_CALL(
      *mock_client_,
      Mount(
          base::FilePath(kShareUrl),
          AllOf(
              Field(&SmbProviderClient::MountOptions::username, kTestADUser),
              Field(&SmbProviderClient::MountOptions::workgroup,
                    base::ToUpperASCII(kTestADDomain)),
              Field(&SmbProviderClient::MountOptions::skip_connect, true),
              Field(&SmbProviderClient::MountOptions::restore_password, false)),
          _, _))
      .WillOnce(WithArg<3>(
          Invoke([&run_loop](SmbProviderClient::MountCallback callback) {
            std::move(callback).Run(smbprovider::ErrorType::ERROR_OK, 7);
            run_loop.Quit();
          })));

  CreateService(ad_profile_);
  run_loop.Run();

  // Because the mock is potentially leaked, expectations needs to be manually
  // verified.
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(mock_client_));
}

TEST_F(SmbServiceTest, Remount_SavedUser) {
  file_system_provider::MountOptions mount_options(
      CreateFileSystemIdForUser(base::FilePath(kSharePath),
                                base::StrCat({kTestUser, "@", kTestDomain})),
      "Foo");
  ProvidedFileSystemInfo file_system_info(
      kProviderId, mount_options, base::FilePath(kSharePath),
      false /* configurable */, false /* watchable */,
      extensions::SOURCE_NETWORK, chromeos::file_system_provider::IconSet());
  CreateFspRegistry(profile_);
  registry_->RememberFileSystem(file_system_info, {});

  base::RunLoop run_loop;
  EXPECT_CALL(
      *mock_client_,
      Mount(
          base::FilePath(kShareUrl),
          AllOf(Field(&SmbProviderClient::MountOptions::username, kTestUser),
                Field(&SmbProviderClient::MountOptions::workgroup, kTestDomain),
                Field(&SmbProviderClient::MountOptions::skip_connect, true),
                Field(&SmbProviderClient::MountOptions::restore_password, true),
                Field(&SmbProviderClient::MountOptions::account_hash, Ne(""))),
          _, _))
      .WillOnce(WithArg<3>(
          Invoke([&run_loop](SmbProviderClient::MountCallback callback) {
            std::move(callback).Run(smbprovider::ErrorType::ERROR_OK, 7);
            run_loop.Quit();
          })));

  CreateService(profile_);
  run_loop.Run();

  // Because the mock is potentially leaked, expectations needs to be manually
  // verified.
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(mock_client_));
}

TEST_F(SmbServiceTest, Remount_SavedInvalidUser) {
  file_system_provider::MountOptions mount_options(
      CreateFileSystemIdForUser(
          base::FilePath(kSharePath),
          base::StrCat({kTestUser, "@", kTestDomain, "@", kTestDomain})),
      "Foo");
  ProvidedFileSystemInfo file_system_info(
      kProviderId, mount_options, base::FilePath(kSharePath),
      false /* configurable */, false /* watchable */,
      extensions::SOURCE_NETWORK, chromeos::file_system_provider::IconSet());
  CreateFspRegistry(profile_);
  registry_->RememberFileSystem(file_system_info, {});

  base::RunLoop run_loop;
  EXPECT_CALL(
      *mock_client_,
      Mount(base::FilePath(kShareUrl),
            AllOf(Field(&SmbProviderClient::MountOptions::username, ""),
                  Field(&SmbProviderClient::MountOptions::workgroup, ""),
                  Field(&SmbProviderClient::MountOptions::skip_connect, true),
                  Field(&SmbProviderClient::MountOptions::restore_password,
                        false)),
            _, _))
      .WillOnce(WithArg<3>(
          Invoke([&run_loop](SmbProviderClient::MountCallback callback) {
            std::move(callback).Run(smbprovider::ErrorType::ERROR_OK, 7);
            run_loop.Quit();
          })));

  CreateService(profile_);
  run_loop.Run();

  // Because the mock is potentially leaked, expectations needs to be manually
  // verified.
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(mock_client_));
}

TEST_F(SmbServiceTest, Premount) {
  const char kPremountPath[] = "smb://server/foobar";
  const char kPreconfiguredShares[] =
      "[{\"mode\":\"pre_mount\",\"share_url\":\"\\\\\\\\server\\\\foobar\"}]";
  auto parsed_shares = base::JSONReader::Read(kPreconfiguredShares);
  ASSERT_TRUE(parsed_shares);
  profile_->GetPrefs()->Set(prefs::kNetworkFileSharesPreconfiguredShares,
                            *parsed_shares);

  base::RunLoop run_loop;
  EXPECT_CALL(
      *mock_client_,
      Mount(base::FilePath(kPremountPath),
            AllOf(Field(&SmbProviderClient::MountOptions::username, ""),
                  Field(&SmbProviderClient::MountOptions::workgroup, ""),
                  Field(&SmbProviderClient::MountOptions::skip_connect, true)),
            _, _))
      .WillOnce(WithArg<3>(
          Invoke([&run_loop](SmbProviderClient::MountCallback callback) {
            std::move(callback).Run(smbprovider::ErrorType::ERROR_OK, 7);
            run_loop.Quit();
          })));

  CreateFspRegistry(profile_);
  CreateService(profile_);
  run_loop.Run();

  // Because the mock is potentially leaked, expectations needs to be manually
  // verified.
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(mock_client_));
}

}  // namespace smb_client
}  // namespace chromeos
