// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/feature_state_manager_impl.h"

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/fake_feature_state_manager.h"
#include "chromeos/services/multidevice_setup/fake_host_status_provider.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_android_sms_pairing_state_tracker.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

multidevice::RemoteDeviceRef CreateTestHostDevice() {
  multidevice::RemoteDeviceRef host_device =
      multidevice::CreateRemoteDeviceRefForTest();

  // Set all host features to supported.
  multidevice::RemoteDevice* raw_device =
      multidevice::GetMutableRemoteDevice(host_device);
  raw_device
      ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kSupported;
  raw_device->software_features[multidevice::SoftwareFeature::kSmartLockHost] =
      multidevice::SoftwareFeatureState::kSupported;
  raw_device
      ->software_features[multidevice::SoftwareFeature::kInstantTetheringHost] =
      multidevice::SoftwareFeatureState::kSupported;
  raw_device
      ->software_features[multidevice::SoftwareFeature::kMessagesForWebHost] =
      multidevice::SoftwareFeatureState::kSupported;

  return host_device;
}

}  // namespace

class MultiDeviceSetupFeatureStateManagerImplTest : public testing::Test {
 protected:
  MultiDeviceSetupFeatureStateManagerImplTest()
      : test_local_device_(multidevice::CreateRemoteDeviceRefForTest()),
        test_host_device_(CreateTestHostDevice()) {}
  ~MultiDeviceSetupFeatureStateManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    user_prefs::PrefRegistrySyncable* registry = test_pref_service_->registry();
    RegisterFeaturePrefs(registry);

    fake_host_status_provider_ = std::make_unique<FakeHostStatusProvider>();

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_synced_devices(
        multidevice::RemoteDeviceRefList{test_local_device_,
                                         test_host_device_});
    fake_device_sync_client_->set_local_device_metadata(test_local_device_);

    fake_android_sms_pairing_state_tracker_ =
        std::make_unique<FakeAndroidSmsPairingStateTracker>();
    fake_android_sms_pairing_state_tracker_->SetPairingComplete(true);

    manager_ = FeatureStateManagerImpl::Factory::Create(
        test_pref_service_.get(), fake_host_status_provider_.get(),
        fake_device_sync_client_.get(),
        fake_android_sms_pairing_state_tracker_.get());

    fake_observer_ = std::make_unique<FakeFeatureStateManagerObserver>();
    manager_->AddObserver(fake_observer_.get());
  }

  void TryAllUnverifiedHostStatesAndVerifyFeatureState(mojom::Feature feature) {
    bool was_previously_verified =
        fake_host_status_provider_->GetHostWithStatus().host_status() ==
        mojom::HostStatus::kHostVerified;
    size_t num_observer_events_before_call =
        fake_observer_->feature_state_updates().size();
    size_t expected_num_observer_events_after_call =
        num_observer_events_before_call + (was_previously_verified ? 1u : 0u);

    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kNoEligibleHosts, base::nullopt /* host_device */);
    if (was_previously_verified) {
      VerifyFeatureStateChange(num_observer_events_before_call, feature,
                               mojom::FeatureState::kUnavailableNoVerifiedHost);
    }
    EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost,
              manager_->GetFeatureStates()[feature]);
    EXPECT_EQ(expected_num_observer_events_after_call,
              fake_observer_->feature_state_updates().size());

    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kEligibleHostExistsButNoHostSet,
        base::nullopt /* host_device */);
    EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost,
              manager_->GetFeatureStates()[feature]);
    EXPECT_EQ(expected_num_observer_events_after_call,
              fake_observer_->feature_state_updates().size());

    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
        test_host_device_);
    EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost,
              manager_->GetFeatureStates()[feature]);
    EXPECT_EQ(expected_num_observer_events_after_call,
              fake_observer_->feature_state_updates().size());

    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kHostSetButNotYetVerified, test_host_device_);
    EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost,
              manager_->GetFeatureStates()[feature]);
    EXPECT_EQ(expected_num_observer_events_after_call,
              fake_observer_->feature_state_updates().size());
  }

  void SetVerifiedHost() {
    // Should not already be verified if we are setting it to be verified.
    EXPECT_NE(mojom::HostStatus::kHostVerified,
              fake_host_status_provider_->GetHostWithStatus().host_status());

    size_t num_observer_events_before_call =
        fake_observer_->feature_state_updates().size();

    SetSoftwareFeatureState(false /* use_local_device */,
                            multidevice::SoftwareFeature::kBetterTogetherHost,
                            multidevice::SoftwareFeatureState::kEnabled);
    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kHostVerified, test_host_device_);

    // Since the host is now verified, there should be a feature state update
    // for all features.
    EXPECT_EQ(num_observer_events_before_call + 1u,
              fake_observer_->feature_state_updates().size());
  }

  void MakeBetterTogetherSuiteDisabledByUser() {
    SetSoftwareFeatureState(true /* use_local_device */,
                            multidevice::SoftwareFeature::kBetterTogetherClient,
                            multidevice::SoftwareFeatureState::kSupported);
    test_pref_service_->SetBoolean(kBetterTogetherSuiteEnabledPrefName, false);
    EXPECT_EQ(
        mojom::FeatureState::kDisabledByUser,
        manager_->GetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);
  }

  void VerifyFeatureStateChange(size_t expected_index,
                                mojom::Feature expected_feature,
                                mojom::FeatureState expected_feature_state) {
    const FeatureStateManager::FeatureStatesMap& map =
        fake_observer_->feature_state_updates()[expected_index];
    const auto it = map.find(expected_feature);
    EXPECT_NE(map.end(), it);
    EXPECT_EQ(expected_feature_state, it->second);
  }

  void SetSoftwareFeatureState(
      bool use_local_device,
      multidevice::SoftwareFeature software_feature,
      multidevice::SoftwareFeatureState software_feature_state) {
    multidevice::RemoteDeviceRef& device =
        use_local_device ? test_local_device_ : test_host_device_;
    multidevice::GetMutableRemoteDevice(device)
        ->software_features[software_feature] = software_feature_state;
    fake_device_sync_client_->NotifyNewDevicesSynced();
  }

  void SetAndroidSmsPairingState(bool is_paired) {
    fake_android_sms_pairing_state_tracker_->SetPairingComplete(is_paired);
  }

  sync_preferences::TestingPrefServiceSyncable* test_pref_service() {
    return test_pref_service_.get();
  }

  FeatureStateManager* manager() { return manager_.get(); }

 private:
  multidevice::RemoteDeviceRef test_local_device_;
  multidevice::RemoteDeviceRef test_host_device_;

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<FakeHostStatusProvider> fake_host_status_provider_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<FakeAndroidSmsPairingStateTracker>
      fake_android_sms_pairing_state_tracker_;

  std::unique_ptr<FakeFeatureStateManagerObserver> fake_observer_;

  std::unique_ptr<FeatureStateManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupFeatureStateManagerImplTest);
};

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, BetterTogetherSuite) {
  TryAllUnverifiedHostStatesAndVerifyFeatureState(
      mojom::Feature::kBetterTogetherSuite);

  SetVerifiedHost();
  EXPECT_EQ(
      mojom::FeatureState::kNotSupportedByChromebook,
      manager()->GetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);

  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kBetterTogetherClient,
                          multidevice::SoftwareFeatureState::kSupported);
  EXPECT_EQ(
      mojom::FeatureState::kEnabledByUser,
      manager()->GetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);
  VerifyFeatureStateChange(1u /* expected_index */,
                           mojom::Feature::kBetterTogetherSuite,
                           mojom::FeatureState::kEnabledByUser);

  test_pref_service()->SetBoolean(kBetterTogetherSuiteEnabledPrefName, false);
  EXPECT_EQ(
      mojom::FeatureState::kDisabledByUser,
      manager()->GetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);
  VerifyFeatureStateChange(2u /* expected_index */,
                           mojom::Feature::kBetterTogetherSuite,
                           mojom::FeatureState::kDisabledByUser);

  // Set all features to prohibited. This should cause the Better Together suite
  // to become prohibited as well.
  test_pref_service()->SetBoolean(kInstantTetheringAllowedPrefName, false);
  test_pref_service()->SetBoolean(kMessagesAllowedPrefName, false);
  test_pref_service()->SetBoolean(kSmartLockAllowedPrefName, false);
  EXPECT_EQ(
      mojom::FeatureState::kProhibitedByPolicy,
      manager()->GetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);
  VerifyFeatureStateChange(5u /* expected_index */,
                           mojom::Feature::kBetterTogetherSuite,
                           mojom::FeatureState::kProhibitedByPolicy);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, InstantTethering) {
  TryAllUnverifiedHostStatesAndVerifyFeatureState(
      mojom::Feature::kInstantTethering);

  SetVerifiedHost();
  EXPECT_EQ(mojom::FeatureState::kNotSupportedByChromebook,
            manager()->GetFeatureStates()[mojom::Feature::kInstantTethering]);

  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kInstantTetheringClient,
                          multidevice::SoftwareFeatureState::kSupported);
  EXPECT_EQ(mojom::FeatureState::kNotSupportedByPhone,
            manager()->GetFeatureStates()[mojom::Feature::kInstantTethering]);
  VerifyFeatureStateChange(1u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kNotSupportedByPhone);

  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kInstantTetheringHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  EXPECT_EQ(mojom::FeatureState::kEnabledByUser,
            manager()->GetFeatureStates()[mojom::Feature::kInstantTethering]);
  VerifyFeatureStateChange(2u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kEnabledByUser);

  MakeBetterTogetherSuiteDisabledByUser();
  EXPECT_EQ(mojom::FeatureState::kUnavailableSuiteDisabled,
            manager()->GetFeatureStates()[mojom::Feature::kInstantTethering]);
  VerifyFeatureStateChange(4u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kUnavailableSuiteDisabled);

  test_pref_service()->SetBoolean(kInstantTetheringEnabledPrefName, false);
  EXPECT_EQ(mojom::FeatureState::kDisabledByUser,
            manager()->GetFeatureStates()[mojom::Feature::kInstantTethering]);
  VerifyFeatureStateChange(5u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kDisabledByUser);

  test_pref_service()->SetBoolean(kInstantTetheringAllowedPrefName, false);
  EXPECT_EQ(mojom::FeatureState::kProhibitedByPolicy,
            manager()->GetFeatureStates()[mojom::Feature::kInstantTethering]);
  VerifyFeatureStateChange(6u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kProhibitedByPolicy);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, Messages) {
  TryAllUnverifiedHostStatesAndVerifyFeatureState(mojom::Feature::kMessages);

  SetVerifiedHost();
  EXPECT_EQ(mojom::FeatureState::kNotSupportedByChromebook,
            manager()->GetFeatureStates()[mojom::Feature::kMessages]);

  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kMessagesForWebClient,
                          multidevice::SoftwareFeatureState::kSupported);
  EXPECT_EQ(mojom::FeatureState::kNotSupportedByPhone,
            manager()->GetFeatureStates()[mojom::Feature::kMessages]);
  VerifyFeatureStateChange(1u /* expected_index */, mojom::Feature::kMessages,
                           mojom::FeatureState::kNotSupportedByPhone);

  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kMessagesForWebHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  EXPECT_EQ(mojom::FeatureState::kEnabledByUser,
            manager()->GetFeatureStates()[mojom::Feature::kMessages]);
  VerifyFeatureStateChange(2u /* expected_index */, mojom::Feature::kMessages,
                           mojom::FeatureState::kEnabledByUser);

  SetAndroidSmsPairingState(false /* is_paired */);
  EXPECT_EQ(mojom::FeatureState::kFurtherSetupRequired,
            manager()->GetFeatureStates()[mojom::Feature::kMessages]);
  VerifyFeatureStateChange(3u /* expected_index */, mojom::Feature::kMessages,
                           mojom::FeatureState::kFurtherSetupRequired);

  SetAndroidSmsPairingState(true /* is_paired */);
  EXPECT_EQ(mojom::FeatureState::kEnabledByUser,
            manager()->GetFeatureStates()[mojom::Feature::kMessages]);
  VerifyFeatureStateChange(4u /* expected_index */, mojom::Feature::kMessages,
                           mojom::FeatureState::kEnabledByUser);

  SetAndroidSmsPairingState(false /* is_paired */);
  MakeBetterTogetherSuiteDisabledByUser();
  EXPECT_EQ(mojom::FeatureState::kUnavailableSuiteDisabled,
            manager()->GetFeatureStates()[mojom::Feature::kMessages]);
  VerifyFeatureStateChange(7u /* expected_index */, mojom::Feature::kMessages,
                           mojom::FeatureState::kUnavailableSuiteDisabled);

  SetAndroidSmsPairingState(true /* is_paired */);
  EXPECT_EQ(mojom::FeatureState::kUnavailableSuiteDisabled,
            manager()->GetFeatureStates()[mojom::Feature::kMessages]);

  test_pref_service()->SetBoolean(kMessagesEnabledPrefName, false);
  EXPECT_EQ(mojom::FeatureState::kDisabledByUser,
            manager()->GetFeatureStates()[mojom::Feature::kMessages]);
  VerifyFeatureStateChange(8u /* expected_index */, mojom::Feature::kMessages,
                           mojom::FeatureState::kDisabledByUser);

  test_pref_service()->SetBoolean(kMessagesAllowedPrefName, false);
  EXPECT_EQ(mojom::FeatureState::kProhibitedByPolicy,
            manager()->GetFeatureStates()[mojom::Feature::kMessages]);
  VerifyFeatureStateChange(9u /* expected_index */, mojom::Feature::kMessages,
                           mojom::FeatureState::kProhibitedByPolicy);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, SmartLock) {
  TryAllUnverifiedHostStatesAndVerifyFeatureState(mojom::Feature::kSmartLock);

  SetVerifiedHost();
  EXPECT_EQ(mojom::FeatureState::kNotSupportedByChromebook,
            manager()->GetFeatureStates()[mojom::Feature::kSmartLock]);

  SetSoftwareFeatureState(true /* use_local_device */,
                          multidevice::SoftwareFeature::kSmartLockClient,
                          multidevice::SoftwareFeatureState::kSupported);
  EXPECT_EQ(mojom::FeatureState::kUnavailableInsufficientSecurity,
            manager()->GetFeatureStates()[mojom::Feature::kSmartLock]);
  VerifyFeatureStateChange(
      1u /* expected_index */, mojom::Feature::kSmartLock,
      mojom::FeatureState::kUnavailableInsufficientSecurity);

  SetSoftwareFeatureState(false /* use_local_device */,
                          multidevice::SoftwareFeature::kSmartLockHost,
                          multidevice::SoftwareFeatureState::kEnabled);
  EXPECT_EQ(mojom::FeatureState::kEnabledByUser,
            manager()->GetFeatureStates()[mojom::Feature::kSmartLock]);
  VerifyFeatureStateChange(2u /* expected_index */, mojom::Feature::kSmartLock,
                           mojom::FeatureState::kEnabledByUser);

  MakeBetterTogetherSuiteDisabledByUser();
  EXPECT_EQ(mojom::FeatureState::kUnavailableSuiteDisabled,
            manager()->GetFeatureStates()[mojom::Feature::kSmartLock]);
  VerifyFeatureStateChange(4u /* expected_index */, mojom::Feature::kSmartLock,
                           mojom::FeatureState::kUnavailableSuiteDisabled);

  test_pref_service()->SetBoolean(kSmartLockEnabledPrefName, false);
  EXPECT_EQ(mojom::FeatureState::kDisabledByUser,
            manager()->GetFeatureStates()[mojom::Feature::kSmartLock]);
  VerifyFeatureStateChange(5u /* expected_index */, mojom::Feature::kSmartLock,
                           mojom::FeatureState::kDisabledByUser);

  test_pref_service()->SetBoolean(kSmartLockAllowedPrefName, false);
  EXPECT_EQ(mojom::FeatureState::kProhibitedByPolicy,
            manager()->GetFeatureStates()[mojom::Feature::kSmartLock]);
  VerifyFeatureStateChange(6u /* expected_index */, mojom::Feature::kSmartLock,
                           mojom::FeatureState::kProhibitedByPolicy);
}

}  // namespace multidevice_setup

}  // namespace chromeos
