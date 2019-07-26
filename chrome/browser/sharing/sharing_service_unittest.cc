// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_service.h"

#include <memory>
#include <vector>

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/sharing/fake_local_device_info_provider.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_device_registration.h"
#include "chrome/browser/sharing/sharing_device_registration_result.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "components/gcm_driver/crypto/gcm_encryption_provider.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::ScopedTaskEnvironment;
using namespace instance_id;
using namespace testing;

namespace {

constexpr int kNoCapabilities =
    static_cast<int>(SharingDeviceCapability::kNone);
const char kP256dh[] = "p256dh";
const char kAuthSecret[] = "auth_secret";
const char kFcmToken[] = "fcm_token";
const char kDeviceName[] = "other_name";
const char kMessageId[] = "message_id";
const char kAuthorizedEntity[] = "authorized_entity";
constexpr base::TimeDelta kTtl = base::TimeDelta::FromSeconds(10);

class FakeGCMDriver : public gcm::FakeGCMDriver {
 public:
  FakeGCMDriver() {}
  ~FakeGCMDriver() override {}

  void SendWebPushMessage(
      const std::string& app_id,
      const std::string& authorized_entity,
      const std::string& p256dh,
      const std::string& auth_secret,
      const std::string& fcm_token,
      crypto::ECPrivateKey* vapid_key,
      gcm::WebPushMessage message,
      gcm::GCMDriver::SendWebPushMessageCallback callback) override {
    p256dh_ = p256dh;
    auth_secret_ = auth_secret;
    fcm_token_ = fcm_token;
    if (should_respond_)
      std::move(callback).Run(base::make_optional(kMessageId));
  }

  gcm::GCMEncryptionProvider* GetEncryptionProviderInternal() override {
    return nullptr;
  }

  void set_should_respond(bool should_respond) {
    should_respond_ = should_respond;
  }

  const std::string& p256dh() { return p256dh_; }
  const std::string& auth_secret() { return auth_secret_; }
  const std::string& fcm_token() { return fcm_token_; }

 private:
  std::string p256dh_, auth_secret_, fcm_token_;
  bool should_respond_ = true;
};

class MockInstanceIDDriver : public InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}
  ~MockInstanceIDDriver() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInstanceIDDriver);
};

class MockSharingFCMHandler : public SharingFCMHandler {
  using SharingMessage = chrome_browser_sharing::SharingMessage;

 public:
  MockSharingFCMHandler() : SharingFCMHandler(nullptr, nullptr) {}
  ~MockSharingFCMHandler() = default;

  MOCK_METHOD0(StartListening, void());
  MOCK_METHOD0(StopListening, void());

  void AddSharingHandler(const SharingMessage::PayloadCase& payload_case,
                         SharingMessageHandler* handler) override {
    sharing_handlers_[payload_case] = handler;
  }

  void RemoveSharingHandler(
      const SharingMessage::PayloadCase& payload_case) override {
    sharing_handlers_.erase(payload_case);
  }

  SharingMessageHandler* GetSharingHandler(
      const SharingMessage::PayloadCase& payload_case) {
    return sharing_handlers_[payload_case];
  }

 private:
  std::map<SharingMessage::PayloadCase, SharingMessageHandler*>
      sharing_handlers_;
};

class FakeSharingDeviceRegistration : public SharingDeviceRegistration {
 public:
  FakeSharingDeviceRegistration(
      SharingSyncPreference* prefs,
      instance_id::InstanceIDDriver* instance_id_driver,
      VapidKeyManager* vapid_key_manager,
      syncer::LocalDeviceInfoProvider* device_info_tracker)
      : SharingDeviceRegistration(prefs,
                                  instance_id_driver,
                                  vapid_key_manager,
                                  device_info_tracker) {}
  ~FakeSharingDeviceRegistration() override = default;

  void RegisterDevice(
      SharingDeviceRegistration::RegistrationCallback callback) override {
    registration_attempts_++;
    std::move(callback).Run(result_);
  }

  void UnregisterDevice(
      SharingDeviceRegistration::RegistrationCallback callback) override {
    unregistration_attempts_++;
    std::move(callback).Run(result_);
  }

  void SetResult(SharingDeviceRegistrationResult result) { result_ = result; }

  int registration_attempts() { return registration_attempts_; }
  int unregistration_attempts() { return unregistration_attempts_; }

 private:
  SharingDeviceRegistrationResult result_ =
      SharingDeviceRegistrationResult::kSuccess;
  int registration_attempts_ = 0;
  int unregistration_attempts_ = 0;
};

class SharingServiceTest : public testing::Test {
 public:
  SharingServiceTest() {
    sync_prefs_ = new SharingSyncPreference(&prefs_);
    sharing_device_registration_ = new FakeSharingDeviceRegistration(
        sync_prefs_, &mock_instance_id_driver_, vapid_key_manager_,
        &fake_local_device_info_provider_);
    vapid_key_manager_ = new VapidKeyManager(sync_prefs_);
    fcm_sender_ = new SharingFCMSender(&fake_gcm_driver_,
                                       &fake_local_device_info_provider_,
                                       sync_prefs_, vapid_key_manager_);
    fcm_handler_ = new NiceMock<MockSharingFCMHandler>();
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }

  void OnMessageSent(bool success) {
    send_message_success_ = base::make_optional(success);
  }

  base::Optional<bool> send_message_success() { return send_message_success_; }

 protected:
  static std::unique_ptr<syncer::DeviceInfo> CreateFakeDeviceInfo(
      const std::string& id,
      const std::string& name) {
    return std::make_unique<syncer::DeviceInfo>(
        id, name, "chrome_version", "user_agent",
        sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id",
        /* last_updated_timestamp= */ base::Time::Now(),
        /* send_tab_to_self_receiving_enabled= */ false);
  }

  static SharingSyncPreference::Device CreateFakeSyncDevice() {
    return SharingSyncPreference::Device(kFcmToken, kP256dh, kAuthSecret,
                                         kNoCapabilities);
  }

  // Lazily initialized so we can test the constructor.
  SharingService* GetSharingService() {
    if (!sharing_service_) {
      sharing_service_ = std::make_unique<SharingService>(
          base::WrapUnique(sync_prefs_), base::WrapUnique(vapid_key_manager_),
          base::WrapUnique(sharing_device_registration_),
          base::WrapUnique(fcm_sender_), base::WrapUnique(fcm_handler_),
          &fake_gcm_driver_, &device_info_tracker_,
          &fake_local_device_info_provider_, &test_sync_service_);
    }
    return sharing_service_.get();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::TestBrowserThreadBundle scoped_task_environment_{
      ScopedTaskEnvironment::TimeSource::MOCK_TIME_AND_NOW};

  syncer::FakeDeviceInfoTracker device_info_tracker_;
  FakeLocalDeviceInfoProvider fake_local_device_info_provider_;
  syncer::TestSyncService test_sync_service_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  FakeGCMDriver fake_gcm_driver_;

  NiceMock<MockInstanceIDDriver> mock_instance_id_driver_;
  NiceMock<MockSharingFCMHandler>* fcm_handler_;

  SharingSyncPreference* sync_prefs_;
  VapidKeyManager* vapid_key_manager_;
  FakeSharingDeviceRegistration* sharing_device_registration_;
  SharingFCMSender* fcm_sender_;

 private:
  std::unique_ptr<SharingService> sharing_service_ = nullptr;
  base::Optional<bool> send_message_success_;
};

}  // namespace

TEST_F(SharingServiceTest, GetDeviceCandidates_Empty) {
  std::vector<SharingDeviceInfo> candidates =
      GetSharingService()->GetDeviceCandidates(kNoCapabilities);
  EXPECT_TRUE(candidates.empty());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_NoSynced) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(id, kDeviceName);
  device_info_tracker_.Add(device_info.get());

  std::vector<SharingDeviceInfo> candidates =
      GetSharingService()->GetDeviceCandidates(kNoCapabilities);

  EXPECT_TRUE(candidates.empty());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_NoTracked) {
  sync_prefs_->SetSyncDevice(base::GenerateGUID(), CreateFakeSyncDevice());

  std::vector<SharingDeviceInfo> candidates =
      GetSharingService()->GetDeviceCandidates(kNoCapabilities);

  EXPECT_TRUE(candidates.empty());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_SyncedAndTracked) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(id, kDeviceName);
  device_info_tracker_.Add(device_info.get());
  sync_prefs_->SetSyncDevice(id, CreateFakeSyncDevice());

  std::vector<SharingDeviceInfo> candidates =
      GetSharingService()->GetDeviceCandidates(kNoCapabilities);

  ASSERT_EQ(1u, candidates.size());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_Expired) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(id, kDeviceName);
  device_info_tracker_.Add(device_info.get());
  sync_prefs_->SetSyncDevice(id, CreateFakeSyncDevice());

  // Forward time until device expires.
  scoped_task_environment_.FastForwardBy(kDeviceExpiration +
                                         base::TimeDelta::FromMilliseconds(1));

  std::vector<SharingDeviceInfo> candidates =
      GetSharingService()->GetDeviceCandidates(kNoCapabilities);

  EXPECT_TRUE(candidates.empty());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_MissingRequirements) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(id, kDeviceName);
  device_info_tracker_.Add(device_info.get());
  sync_prefs_->SetSyncDevice(id, CreateFakeSyncDevice());

  // Require all capabilities.
  std::vector<SharingDeviceInfo> candidates =
      GetSharingService()->GetDeviceCandidates(-1);

  EXPECT_TRUE(candidates.empty());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_DuplicateDeviceNames) {
  // Add first device.
  std::string id1 = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info_1 =
      CreateFakeDeviceInfo(id1, kDeviceName);
  device_info_tracker_.Add(device_info_1.get());
  sync_prefs_->SetSyncDevice(id1, CreateFakeSyncDevice());

  // Advance time for a bit to create a newer device.
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));

  // Add second device.
  std::string id2 = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info_2 =
      CreateFakeDeviceInfo(id2, kDeviceName);
  device_info_tracker_.Add(device_info_2.get());
  sync_prefs_->SetSyncDevice(id2, CreateFakeSyncDevice());

  // Add third device which is same as local device ("name").
  std::string id3 = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info_3 =
      CreateFakeDeviceInfo(id3, "name");
  device_info_tracker_.Add(device_info_3.get());
  sync_prefs_->SetSyncDevice(id3, CreateFakeSyncDevice());

  std::vector<SharingDeviceInfo> candidates =
      GetSharingService()->GetDeviceCandidates(kNoCapabilities);

  ASSERT_EQ(1u, candidates.size());
  EXPECT_EQ(id2, candidates[0].guid());
}

TEST_F(SharingServiceTest, SendMessageToDeviceSuccess) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(id, kDeviceName);
  device_info_tracker_.Add(device_info.get());
  sync_prefs_->SetSyncDevice(id, CreateFakeSyncDevice());
  sync_prefs_->SetFCMRegistration({kAuthorizedEntity, "", base::Time::Now()});

  GetSharingService()->SendMessageToDevice(
      id, kTtl, chrome_browser_sharing::SharingMessage(),
      base::BindOnce(&SharingServiceTest::OnMessageSent,
                     base::Unretained(this)));

  EXPECT_EQ(kP256dh, fake_gcm_driver_.p256dh());
  EXPECT_EQ(kAuthSecret, fake_gcm_driver_.auth_secret());
  EXPECT_EQ(kFcmToken, fake_gcm_driver_.fcm_token());

  // Simulate ack message received by AckMessageHandler.
  SharingMessageHandler* ack_message_handler = fcm_handler_->GetSharingHandler(
      chrome_browser_sharing::SharingMessage::kAckMessage);
  EXPECT_TRUE(ack_message_handler);

  chrome_browser_sharing::SharingMessage ack_message;
  ack_message.mutable_ack_message()->set_original_message_id(kMessageId);
  ack_message_handler->OnMessage(ack_message);

  EXPECT_TRUE(send_message_success().has_value());
  EXPECT_TRUE(*send_message_success());
}

TEST_F(SharingServiceTest, SendMessageToDeviceFCMNotResponding) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(id, kDeviceName);
  device_info_tracker_.Add(device_info.get());
  sync_prefs_->SetSyncDevice(id, CreateFakeSyncDevice());
  sync_prefs_->SetFCMRegistration({kAuthorizedEntity, "", base::Time::Now()});

  // FCM driver will not respond to the send request.
  fake_gcm_driver_.set_should_respond(false);

  GetSharingService()->SendMessageToDevice(
      id, kTtl, chrome_browser_sharing::SharingMessage(),
      base::BindOnce(&SharingServiceTest::OnMessageSent,
                     base::Unretained(this)));

  EXPECT_EQ(kP256dh, fake_gcm_driver_.p256dh());
  EXPECT_EQ(kAuthSecret, fake_gcm_driver_.auth_secret());
  EXPECT_EQ(kFcmToken, fake_gcm_driver_.fcm_token());

  // Advance time so send message will expire.
  scoped_task_environment_.FastForwardBy(kSendMessageTimeout);
  EXPECT_TRUE(send_message_success().has_value());
  EXPECT_FALSE(*send_message_success());

  // Simulate ack message received by AckMessageHandler, which will be
  // disregarded.
  SharingMessageHandler* ack_message_handler = fcm_handler_->GetSharingHandler(
      chrome_browser_sharing::SharingMessage::kAckMessage);
  EXPECT_TRUE(ack_message_handler);

  chrome_browser_sharing::SharingMessage ack_message;
  ack_message.mutable_ack_message()->set_original_message_id(kMessageId);
  ack_message_handler->OnMessage(ack_message);

  EXPECT_TRUE(send_message_success().has_value());
  EXPECT_FALSE(*send_message_success());
}

TEST_F(SharingServiceTest, SendMessageToDeviceExpired) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(id, kDeviceName);
  device_info_tracker_.Add(device_info.get());
  sync_prefs_->SetSyncDevice(id, CreateFakeSyncDevice());
  sync_prefs_->SetFCMRegistration({kAuthorizedEntity, "", base::Time::Now()});

  GetSharingService()->SendMessageToDevice(
      id, kTtl, chrome_browser_sharing::SharingMessage(),
      base::BindOnce(&SharingServiceTest::OnMessageSent,
                     base::Unretained(this)));

  EXPECT_EQ(kP256dh, fake_gcm_driver_.p256dh());
  EXPECT_EQ(kAuthSecret, fake_gcm_driver_.auth_secret());
  EXPECT_EQ(kFcmToken, fake_gcm_driver_.fcm_token());

  // Advance time so send message will expire.
  scoped_task_environment_.FastForwardBy(kSendMessageTimeout);
  EXPECT_TRUE(send_message_success().has_value());
  EXPECT_FALSE(*send_message_success());

  // Simulate ack message received by AckMessageHandler, which will be
  // disregarded.
  SharingMessageHandler* ack_message_handler = fcm_handler_->GetSharingHandler(
      chrome_browser_sharing::SharingMessage::kAckMessage);
  EXPECT_TRUE(ack_message_handler);

  chrome_browser_sharing::SharingMessage ack_message;
  ack_message.mutable_ack_message()->set_original_message_id(kMessageId);
  ack_message_handler->OnMessage(ack_message);

  EXPECT_TRUE(send_message_success().has_value());
  EXPECT_FALSE(*send_message_success());
}

TEST_F(SharingServiceTest, DeviceRegistration) {
  // Enable the feature.
  scoped_feature_list_.InitAndEnableFeature(kSharingDeviceRegistration);
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes(syncer::PREFERENCES);

  EXPECT_EQ(SharingService::State::DISABLED, GetSharingService()->GetState());

  // Expect registration to be successful on sync state changed.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kSuccess);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE, GetSharingService()->GetState());

  // As device is already registered, won't attempt registration anymore.
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(0);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE, GetSharingService()->GetState());

  // Registration will be attempeted as VAPID key is cleared.
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(0);
  prefs_.SetUserPref("sharing.vapid_key",
                     base::Value::ToUniquePtrValue(
                         base::Value(base::Value::Type::DICTIONARY)));
  EXPECT_EQ(2, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE, GetSharingService()->GetState());
}

TEST_F(SharingServiceTest, DeviceRegistrationTransientError) {
  // Enable the feature.
  scoped_feature_list_.InitAndEnableFeature(kSharingDeviceRegistration);
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes(syncer::PREFERENCES);

  EXPECT_EQ(SharingService::State::DISABLED, GetSharingService()->GetState());

  // Retry will be scheduled on transient error received.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kFcmTransientError);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::REGISTERING,
            GetSharingService()->GetState());

  // Retry should be scheduled by now. Next retry after 5 minutes will be
  // successful.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kSuccess);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  scoped_task_environment_.FastForwardBy(
      base::TimeDelta::FromMilliseconds(kRetryBackoffPolicy.initial_delay_ms));
  EXPECT_EQ(2, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE, GetSharingService()->GetState());
}

TEST_F(SharingServiceTest, DeviceUnregistrationFeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kSharingDeviceRegistration);
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kSuccess);

  EXPECT_EQ(SharingService::State::DISABLED, GetSharingService()->GetState());

  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED, GetSharingService()->GetState());

  // Further state changes are ignored.
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED, GetSharingService()->GetState());
}

TEST_F(SharingServiceTest, DeviceUnregistrationSyncDisabled) {
  scoped_feature_list_.InitAndEnableFeature(kSharingDeviceRegistration);
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::DISABLED);

  // Create new SharingService instance with sync disabled at constructor.
  GetSharingService();
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED, GetSharingService()->GetState());
}

TEST_F(SharingServiceTest, DeviceRegisterAndUnregister) {
  // Enable the feature.
  scoped_feature_list_.InitAndEnableFeature(kSharingDeviceRegistration);
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes(syncer::PREFERENCES);

  // Create new SharingService instance with feature enabled at constructor.
  GetSharingService();
  EXPECT_EQ(SharingService::State::DISABLED, GetSharingService()->GetState());

  // Expect registration to be successful on sync state changed.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kSuccess);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(0, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE, GetSharingService()->GetState());

  // Further state changes do nothing.
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(0);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(0, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE, GetSharingService()->GetState());

  // Change sync to configuring, which will be ignored.
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::CONFIGURING);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(0, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE, GetSharingService()->GetState());

  // Disable sync and un-registration should happen.
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::DISABLED);
  EXPECT_CALL(*fcm_handler_, StopListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED, GetSharingService()->GetState());

  // Further state changes do nothing.
  EXPECT_CALL(*fcm_handler_, StopListening()).Times(0);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED, GetSharingService()->GetState());

  // Should be able to register once again when sync is back on.
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(2, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE, GetSharingService()->GetState());

  // Disable syncing of preference and un-registration should happen.
  test_sync_service_.SetActiveDataTypes(syncer::ModelTypeSet());
  EXPECT_CALL(*fcm_handler_, StopListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(2, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(2, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED, GetSharingService()->GetState());
}

TEST_F(SharingServiceTest, StartListeningToFCMAtConstructor) {
  scoped_feature_list_.InitAndEnableFeature(kSharingDeviceRegistration);
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes(syncer::PREFERENCES);

  // Create new SharingService instance with FCM already registered at
  // constructor.
  sync_prefs_->SetFCMRegistration({kAuthorizedEntity, "", base::Time::Now()});
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  GetSharingService();
}

TEST_F(SharingServiceTest, NoDevicesWhenSyncDisabled) {
  scoped_feature_list_.InitAndEnableFeature(kSharingDeviceRegistration);
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::DISABLED);

  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(id, kDeviceName);
  device_info_tracker_.Add(device_info.get());
  sync_prefs_->SetSyncDevice(id, CreateFakeSyncDevice());

  std::vector<SharingDeviceInfo> candidates =
      GetSharingService()->GetDeviceCandidates(kNoCapabilities);

  ASSERT_EQ(0u, candidates.size());
}
