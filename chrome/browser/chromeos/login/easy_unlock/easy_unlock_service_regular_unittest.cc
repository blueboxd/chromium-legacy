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
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
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
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
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

}  // namespace

class EasyUnlockServiceRegularTest : public testing::Test {
 protected:
  EasyUnlockServiceRegularTest() = default;
  ~EasyUnlockServiceRegularTest() override = default;

  void SetUp() override {
    mock_adapter_ = new testing::NiceMock<MockBluetoothAdapter>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
    EXPECT_CALL(*mock_adapter_, IsPresent())
        .WillRepeatedly(testing::Invoke(
            this, &EasyUnlockServiceRegularTest::is_bluetooth_adapter_present));

    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_pref_service_);
    RegisterLocalState(local_pref_service_.registry());

    fake_secure_channel_client_ =
        std::make_unique<chromeos::secure_channel::FakeSecureChannelClient>();
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->NotifyReady();
    fake_multidevice_setup_client_ = std::make_unique<
        chromeos::multidevice_setup::FakeMultiDeviceSetupClient>();

    TestingProfile::Builder builder;
    profile_ = builder.Build();

    auto fake_chrome_user_manager =
        std::make_unique<chromeos::FakeChromeUserManager>();
    fake_chrome_user_manager_ = fake_chrome_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_chrome_user_manager));
    SetPrimaryUserLoggedIn();

    easy_unlock_service_regular_ = std::make_unique<EasyUnlockServiceRegular>(
        profile_.get(), fake_secure_channel_client_.get(),
        std::make_unique<MockEasyUnlockNotificationController>(),
        fake_device_sync_client_.get(), fake_multidevice_setup_client_.get());
    easy_unlock_service_regular_->Initialize();
  }

  void TearDown() override {
    easy_unlock_service_regular_->Shutdown();
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

  // Must outlive TestingProfiles.
  content::TestBrowserThreadBundle thread_bundle_;

  std::unique_ptr<TestingProfile> profile_;
  chromeos::FakeChromeUserManager* fake_chrome_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<multidevice_setup::MultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  std::unique_ptr<EasyUnlockServiceRegular> easy_unlock_service_regular_;

  std::string profile_gaia_id_;

  bool is_bluetooth_adapter_present_ = true;
  scoped_refptr<testing::NiceMock<MockBluetoothAdapter>> mock_adapter_;

  // PrefService which contains the browser process' local storage.
  TestingPrefServiceSimple local_pref_service_;

 private:
  void SetPrimaryUserLoggedIn() {
    const AccountId account_id(
        AccountId::FromUserEmail(profile_->GetProfileUserName()));
    const user_manager::User* user =
        fake_chrome_user_manager_->AddPublicAccountUser(account_id);
    fake_chrome_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                            false /* browser_restart */,
                                            false /* is_child */);
  }

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockServiceRegularTest);
};

TEST_F(EasyUnlockServiceRegularTest, NoBluetoothNoService) {
  set_is_bluetooth_adapter_present(false);
  EXPECT_FALSE(easy_unlock_service_regular_->IsAllowed());
}

TEST_F(EasyUnlockServiceRegularTest, NotAllowedForEphemeralAccounts) {
  // Only MockUserManager allows for stubbing
  // IsCurrentUserNonCryptohomeDataEphemeral() to return false so we use one
  // here in place of |fake_chrome_user_manager_|. Injecting it into a local
  // ScopedUserManager sets it up as the global UserManager instance.
  auto mock_user_manager =
      std::make_unique<testing::NiceMock<MockUserManager>>();
  ON_CALL(*mock_user_manager, IsCurrentUserNonCryptohomeDataEphemeral())
      .WillByDefault(Return(false));
  auto scoped_user_manager = std::make_unique<user_manager::ScopedUserManager>(
      std::move(mock_user_manager));

  EXPECT_FALSE(easy_unlock_service_regular_->IsAllowed());
}

}  // namespace chromeos
