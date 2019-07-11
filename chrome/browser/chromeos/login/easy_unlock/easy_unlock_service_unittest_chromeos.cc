// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_notification_controller.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_factory.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_regular.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"

using device::MockBluetoothAdapter;
using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace chromeos {

namespace {

const char kTestUserEmail[] = "user@nowhere.com";

class MockEasyUnlockNotificationController
    : public EasyUnlockNotificationController {
 public:
  MockEasyUnlockNotificationController()
      : EasyUnlockNotificationController(nullptr) {}
  ~MockEasyUnlockNotificationController() override {}

  // EasyUnlockNotificationController:
  MOCK_METHOD0(ShowChromebookAddedNotification, void());
  MOCK_METHOD0(ShowPairingChangeNotification, void());
  MOCK_METHOD1(ShowPairingChangeAppliedNotification, void(const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEasyUnlockNotificationController);
};

device_sync::FakeDeviceSyncClient* GetDefaultDeviceSyncClient() {
  static base::NoDestructor<device_sync::FakeDeviceSyncClient> fake_client;
  return fake_client.get();
}

multidevice_setup::FakeMultiDeviceSetupClient*
GetDefaultMultiDeviceSetupClient() {
  static base::NoDestructor<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_client;
  return fake_client.get();
}

// EasyUnlockService factory function injected into testing profiles.
std::unique_ptr<KeyedService> CreateEasyUnlockServiceForTest(
    content::BrowserContext* context) {
  return std::make_unique<EasyUnlockServiceRegular>(
      Profile::FromBrowserContext(context), nullptr /* secure_channel_client */,
      std::make_unique<MockEasyUnlockNotificationController>(),
      GetDefaultDeviceSyncClient(), GetDefaultMultiDeviceSetupClient());
}

}  // namespace

class EasyUnlockServiceTest : public testing::Test {
 protected:
  EasyUnlockServiceTest()
      : mock_user_manager_(new testing::NiceMock<MockUserManager>()),
        scoped_user_manager_(base::WrapUnique(mock_user_manager_)),
        is_bluetooth_adapter_present_(true) {}

  ~EasyUnlockServiceTest() override {}

  void SetUp() override {
    mock_adapter_ = new testing::NiceMock<MockBluetoothAdapter>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
    EXPECT_CALL(*mock_adapter_, IsPresent())
        .WillRepeatedly(testing::Invoke(
            this, &EasyUnlockServiceTest::is_bluetooth_adapter_present));

    ON_CALL(*mock_user_manager_, Shutdown()).WillByDefault(Return());
    ON_CALL(*mock_user_manager_, IsLoggedInAsUserWithGaiaAccount())
        .WillByDefault(Return(true));
    ON_CALL(*mock_user_manager_, IsCurrentUserNonCryptohomeDataEphemeral())
        .WillByDefault(Return(false));

    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_pref_service_);
    RegisterLocalState(local_pref_service_.registry());

    profile_ = SetUpProfile(kTestUserEmail, &profile_gaia_id_);
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  void SetEasyUnlockAllowedPolicy(bool allowed) {
    profile_->GetTestingPrefService()->SetManagedPref(
        prefs::kEasyUnlockAllowed, std::make_unique<base::Value>(allowed));
  }

  void set_is_bluetooth_adapter_present(bool is_present) {
    is_bluetooth_adapter_present_ = is_present;
  }

  bool is_bluetooth_adapter_present() const {
    return is_bluetooth_adapter_present_;
  }

  // Sets up a test profile using the provided |email|. Will generate a unique
  // gaia id and output to |gaia_id|. Returns the created TestingProfile.
  std::unique_ptr<TestingProfile> SetUpProfile(const std::string& email,
                                               std::string* gaia_id) {
    std::unique_ptr<TestingProfile> profile =
        IdentityTestEnvironmentProfileAdaptor::
            CreateProfileForIdentityTestEnvironment(
                {{EasyUnlockServiceFactory::GetInstance(),
                  base::BindRepeating(&CreateEasyUnlockServiceForTest)}});

    // Note: This can simply be a local variable as the test does not need to
    // interact with IdentityTestEnvironment outside of this method.
    IdentityTestEnvironmentProfileAdaptor identity_test_env_adaptor(
        profile.get());
    CoreAccountInfo account_info =
        identity_test_env_adaptor.identity_test_env()->SetPrimaryAccount(email);

    *gaia_id = account_info.gaia;

    mock_user_manager_->AddUser(
        AccountId::FromUserEmailGaiaId(email, *gaia_id));
    profile.get()->set_profile_name(email);

    // Only initialize the service once the profile is completely ready. If done
    // earlier, indirect usage of user_manager::KnownUser would crash.
    EasyUnlockService::Get(profile.get())->Initialize();

    return profile;
  }

  // Must outlive TestingProfiles.
  content::TestBrowserThreadBundle thread_bundle_;

  std::unique_ptr<TestingProfile> profile_;
  std::string profile_gaia_id_;
  MockUserManager* mock_user_manager_;

  user_manager::ScopedUserManager scoped_user_manager_;

  bool is_bluetooth_adapter_present_;
  scoped_refptr<testing::NiceMock<MockBluetoothAdapter>> mock_adapter_;

  // PrefService which contains the browser process' local storage.
  TestingPrefServiceSimple local_pref_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EasyUnlockServiceTest);
};

TEST_F(EasyUnlockServiceTest, NoBluetoothNoService) {
  set_is_bluetooth_adapter_present(false);

  EasyUnlockService* service = EasyUnlockService::Get(profile_.get());
  ASSERT_TRUE(service);

  EXPECT_FALSE(service->IsAllowed());
}

TEST_F(EasyUnlockServiceTest, NotAllowedForEphemeralAccounts) {
  ON_CALL(*mock_user_manager_, IsCurrentUserNonCryptohomeDataEphemeral())
      .WillByDefault(Return(true));

  EXPECT_FALSE(EasyUnlockService::Get(profile_.get())->IsAllowed());
}

TEST_F(EasyUnlockServiceTest, GetAccountId) {
  EXPECT_EQ(AccountId::FromUserEmailGaiaId(kTestUserEmail, profile_gaia_id_),
            EasyUnlockService::Get(profile_.get())->GetAccountId());
}

}  // namespace chromeos
