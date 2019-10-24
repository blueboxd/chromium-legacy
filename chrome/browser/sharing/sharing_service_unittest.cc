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
#include "base/test/task_environment.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_constants.h"
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
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/ec_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kP256dh[] = "p256dh";
const char kAuthSecret[] = "auth_secret";
const char kFcmToken[] = "fcm_token";
const char kDeviceName[] = "other_name";
const char kMessageId[] = "message_id";
const char kAuthorizedEntity[] = "authorized_entity";
constexpr base::TimeDelta kTimeout = base::TimeDelta::FromSeconds(15);
const char kSenderFcmToken[] = "sender_fcm_token";
const char kSenderP256dh[] = "sender_p256dh";
const char kSenderAuthSecret[] = "sender_auth_secret";

class FakeGCMDriver : public gcm::FakeGCMDriver {
 public:
  FakeGCMDriver() {}
  ~FakeGCMDriver() override {}

  void SendWebPushMessage(const std::string& app_id,
                          const std::string& authorized_entity,
                          const std::string& p256dh,
                          const std::string& auth_secret,
                          const std::string& fcm_token,
                          crypto::ECPrivateKey* vapid_key,
                          gcm::WebPushMessage message,
                          gcm::WebPushCallback callback) override {
    p256dh_ = p256dh;
    auth_secret_ = auth_secret;
    fcm_token_ = fcm_token;
    message_ = std::move(message);
    if (should_respond_)
      std::move(callback).Run(gcm::SendWebPushMessageResult::kSuccessful,
                              base::make_optional(kMessageId));
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
  const gcm::WebPushMessage& message() { return message_; }

 private:
  std::string p256dh_, auth_secret_, fcm_token_;
  gcm::WebPushMessage message_;
  bool should_respond_ = true;
};

class MockInstanceIDDriver : public instance_id::InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}
  ~MockInstanceIDDriver() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInstanceIDDriver);
};

class MockSharingFCMHandler : public SharingFCMHandler {
  using SharingMessage = chrome_browser_sharing::SharingMessage;

 public:
  MockSharingFCMHandler() : SharingFCMHandler(nullptr, nullptr, nullptr) {}
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
      PrefService* pref_service,
      SharingSyncPreference* prefs,
      instance_id::InstanceIDDriver* instance_id_driver,
      VapidKeyManager* vapid_key_manager,
      syncer::LocalDeviceInfoProvider* device_info_tracker)
      : SharingDeviceRegistration(pref_service,
                                  prefs,
                                  instance_id_driver,
                                  vapid_key_manager),
        vapid_key_manager_(vapid_key_manager) {}
  ~FakeSharingDeviceRegistration() override = default;

  void RegisterDevice(
      SharingDeviceRegistration::RegistrationCallback callback) override {
    registration_attempts_++;
    // Simulate SharingDeviceRegistration calling GetOrCreateKey.
    vapid_key_manager_->GetOrCreateKey();
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

  bool IsSharedClipboardSupported() const override {
    return shared_clipboard_supported_;
  }

  void SetIsSharedClipboardSupported(bool supported) {
    shared_clipboard_supported_ = supported;
  }

 private:
  VapidKeyManager* vapid_key_manager_;
  SharingDeviceRegistrationResult result_ =
      SharingDeviceRegistrationResult::kSuccess;
  int registration_attempts_ = 0;
  int unregistration_attempts_ = 0;
  bool shared_clipboard_supported_ = false;
};

class SharingServiceTest : public testing::Test {
 public:
  SharingServiceTest() {
    sync_prefs_ =
        new SharingSyncPreference(&prefs_, &fake_device_info_sync_service);
    vapid_key_manager_ = new VapidKeyManager(sync_prefs_, &test_sync_service_);
    sharing_device_registration_ = new FakeSharingDeviceRegistration(
        /* pref_service= */ nullptr, sync_prefs_, &mock_instance_id_driver_,
        vapid_key_manager_,
        fake_device_info_sync_service.GetLocalDeviceInfoProvider());
    fcm_sender_ = new SharingFCMSender(&fake_gcm_driver_, sync_prefs_,
                                       vapid_key_manager_);
    fcm_handler_ = new testing::NiceMock<MockSharingFCMHandler>();
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }

  void OnMessageSent(
      SharingSendMessageResult result,
      std::unique_ptr<chrome_browser_sharing::ResponseMessage> response) {
    send_message_result_ = base::make_optional(result);
    send_message_response_ = std::move(response);
  }

  const base::Optional<SharingSendMessageResult>& send_message_result() {
    return send_message_result_;
  }

  const chrome_browser_sharing::ResponseMessage* send_message_response() {
    return send_message_response_.get();
  }

  void OnDeviceCandidatesInitialized() {
    device_candidates_initialized_ = true;
  }

 protected:
  static std::unique_ptr<syncer::DeviceInfo> CreateFakeDeviceInfo(
      const std::string& id,
      const std::string& name,
      sync_pb::SyncEnums_DeviceType device_type =
          sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
      base::SysInfo::HardwareInfo hardware_info =
          base::SysInfo::HardwareInfo()) {
    return std::make_unique<syncer::DeviceInfo>(
        id, name, "chrome_version", "user_agent", device_type, "device_id",
        hardware_info,
        /*last_updated_timestamp=*/base::Time::Now(),
        /*send_tab_to_self_receiving_enabled=*/false,
        syncer::DeviceInfo::SharingInfo(
            kFcmToken, kP256dh, kAuthSecret,
            std::set<sync_pb::SharingSpecificFields::EnabledFeatures>{
                sync_pb::SharingSpecificFields::CLICK_TO_CALL}));
  }

  static syncer::DeviceInfo::SharingInfo CreateLocalSharingInfo() {
    return syncer::DeviceInfo::SharingInfo(
        kSenderFcmToken, kSenderP256dh, kSenderAuthSecret,
        std::set<sync_pb::SharingSpecificFields::EnabledFeatures>());
  }

  // Lazily initialized so we can test the constructor.
  SharingService* GetSharingService() {
    if (!sharing_service_) {
      sharing_service_ = std::make_unique<SharingService>(
          base::WrapUnique(sync_prefs_), base::WrapUnique(vapid_key_manager_),
          base::WrapUnique(sharing_device_registration_),
          base::WrapUnique(fcm_sender_), base::WrapUnique(fcm_handler_),
          &fake_gcm_driver_,
          fake_device_info_sync_service.GetDeviceInfoTracker(),
          fake_device_info_sync_service.GetLocalDeviceInfoProvider(),
          &test_sync_service_,
          /* notification_display_service= */ nullptr);
    }
    return sharing_service_.get();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service;
  syncer::TestSyncService test_sync_service_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  FakeGCMDriver fake_gcm_driver_;

  testing::NiceMock<MockInstanceIDDriver> mock_instance_id_driver_;
  testing::NiceMock<MockSharingFCMHandler>* fcm_handler_;

  SharingSyncPreference* sync_prefs_;
  VapidKeyManager* vapid_key_manager_;
  FakeSharingDeviceRegistration* sharing_device_registration_;
  SharingFCMSender* fcm_sender_;
  bool device_candidates_initialized_ = false;

 private:
  std::unique_ptr<SharingService> sharing_service_ = nullptr;
  base::Optional<SharingSendMessageResult> send_message_result_;
  std::unique_ptr<chrome_browser_sharing::ResponseMessage>
      send_message_response_;
};

bool ProtoEquals(const google::protobuf::MessageLite& expected,
                 const google::protobuf::MessageLite& actual) {
  std::string expected_serialized, actual_serialized;
  expected.SerializeToString(&expected_serialized);
  actual.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}
}  // namespace

TEST_F(SharingServiceTest, SharedClipboard_IsAdded) {
  sharing_device_registration_->SetIsSharedClipboardSupported(true);
  GetSharingService();
  SharingMessageHandler* shared_clipboard_handler =
      fcm_handler_->GetSharingHandler(
          chrome_browser_sharing::SharingMessage::kSharedClipboardMessage);
  EXPECT_TRUE(shared_clipboard_handler);
}

TEST_F(SharingServiceTest, SharedClipboard_NotAdded) {
  sharing_device_registration_->SetIsSharedClipboardSupported(false);
  GetSharingService();
  SharingMessageHandler* shared_clipboard_handler =
      fcm_handler_->GetSharingHandler(
          chrome_browser_sharing::SharingMessage::kSharedClipboardMessage);
  EXPECT_FALSE(shared_clipboard_handler);
}

TEST_F(SharingServiceTest, GetDeviceCandidates_Empty) {
  std::vector<std::unique_ptr<syncer::DeviceInfo>> candidates =
      GetSharingService()->GetDeviceCandidates(
          sync_pb::SharingSpecificFields::CLICK_TO_CALL);
  EXPECT_TRUE(candidates.empty());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_NoTracked) {
  std::vector<std::unique_ptr<syncer::DeviceInfo>> candidates =
      GetSharingService()->GetDeviceCandidates(
          sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  EXPECT_TRUE(candidates.empty());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_Tracked) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(id, kDeviceName);
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(device_info.get());

  std::vector<std::unique_ptr<syncer::DeviceInfo>> candidates =
      GetSharingService()->GetDeviceCandidates(
          sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  ASSERT_EQ(1u, candidates.size());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_Expired) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(id, kDeviceName);
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(device_info.get());

  // Forward time until device expires.
  task_environment_.FastForwardBy(kDeviceExpiration +
                                  base::TimeDelta::FromMilliseconds(1));

  std::vector<std::unique_ptr<syncer::DeviceInfo>> candidates =
      GetSharingService()->GetDeviceCandidates(
          sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  EXPECT_TRUE(candidates.empty());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_MissingRequirements) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(id, kDeviceName);
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(device_info.get());

  // Requires shared clipboard feature.
  std::vector<std::unique_ptr<syncer::DeviceInfo>> candidates =
      GetSharingService()->GetDeviceCandidates(
          sync_pb::SharingSpecificFields::SHARED_CLIPBOARD);

  EXPECT_TRUE(candidates.empty());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_DuplicateDeviceNames) {
  // Add first device.
  base::SysInfo::HardwareInfo info{"Google", "Pixel C", "serialno"};

  std::string id1 = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info_1 = CreateFakeDeviceInfo(
      id1, "Device1", sync_pb::SyncEnums_DeviceType_TYPE_TABLET, info);
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(
      device_info_1.get());

  // Advance time for a bit to create a newer device.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));

  // Add second device.
  std::string id2 = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info_2 = CreateFakeDeviceInfo(
      id2, "Device2", sync_pb::SyncEnums_DeviceType_TYPE_TABLET, info);
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(
      device_info_2.get());

  // Add third device which is same as local device except guid.
  std::string id3 = base::GenerateGUID();
  const syncer::DeviceInfo* local_device_info =
      fake_device_info_sync_service.GetLocalDeviceInfoProvider()
          ->GetLocalDeviceInfo();
  std::unique_ptr<syncer::DeviceInfo> device_info_3 = CreateFakeDeviceInfo(
      id3, local_device_info->client_name(), local_device_info->device_type(),
      local_device_info->hardware_info());
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(
      device_info_3.get());

  std::vector<std::unique_ptr<syncer::DeviceInfo>> candidates =
      GetSharingService()->GetDeviceCandidates(
          sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  ASSERT_EQ(1u, candidates.size());
  EXPECT_EQ(id2, candidates[0]->guid());
}

TEST_F(SharingServiceTest, SendMessageToDeviceSuccess) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(id, kDeviceName);
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(device_info.get());
  fake_device_info_sync_service.GetLocalDeviceInfoProvider()
      ->GetMutableDeviceInfo()
      ->set_sharing_info(CreateLocalSharingInfo());
  sync_prefs_->SetFCMRegistration(SharingSyncPreference::FCMRegistration(
      kAuthorizedEntity, base::Time::Now()));

  GetSharingService()->SendMessageToDevice(
      id, kTimeout, chrome_browser_sharing::SharingMessage(),
      base::BindOnce(&SharingServiceTest::OnMessageSent,
                     base::Unretained(this)));

  EXPECT_EQ(kP256dh, fake_gcm_driver_.p256dh());
  EXPECT_EQ(kAuthSecret, fake_gcm_driver_.auth_secret());
  EXPECT_EQ(kFcmToken, fake_gcm_driver_.fcm_token());

  chrome_browser_sharing::SharingMessage sharing_message;
  ASSERT_TRUE(
      sharing_message.ParseFromString(fake_gcm_driver_.message().payload));
  EXPECT_EQ("id", sharing_message.sender_guid());
  EXPECT_EQ("manufacturer Computer model",
            sharing_message.sender_device_name());

  // Simulate ack message received by AckMessageHandler.
  SharingMessageHandler* ack_message_handler = fcm_handler_->GetSharingHandler(
      chrome_browser_sharing::SharingMessage::kAckMessage);
  EXPECT_TRUE(ack_message_handler);

  chrome_browser_sharing::SharingMessage ack_message;
  ack_message.mutable_ack_message()->set_original_message_id(kMessageId);
  ack_message.mutable_ack_message()->mutable_response_message();
  ack_message_handler->OnMessage(ack_message, base::DoNothing());

  EXPECT_EQ(SharingSendMessageResult::kSuccessful, send_message_result());
  ASSERT_TRUE(send_message_response());
  EXPECT_TRUE(ProtoEquals(ack_message.ack_message().response_message(),
                          *send_message_response()));
}

TEST_F(SharingServiceTest, SendMessageToDeviceFCMNotResponding) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(id, kDeviceName);
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(device_info.get());
  fake_device_info_sync_service.GetLocalDeviceInfoProvider()
      ->GetMutableDeviceInfo()
      ->set_sharing_info(CreateLocalSharingInfo());
  sync_prefs_->SetFCMRegistration(SharingSyncPreference::FCMRegistration(
      kAuthorizedEntity, base::Time::Now()));

  // FCM driver will not respond to the send request.
  fake_gcm_driver_.set_should_respond(false);

  GetSharingService()->SendMessageToDevice(
      id, kTimeout, chrome_browser_sharing::SharingMessage(),
      base::BindOnce(&SharingServiceTest::OnMessageSent,
                     base::Unretained(this)));

  EXPECT_EQ(kP256dh, fake_gcm_driver_.p256dh());
  EXPECT_EQ(kAuthSecret, fake_gcm_driver_.auth_secret());
  EXPECT_EQ(kFcmToken, fake_gcm_driver_.fcm_token());

  // Advance time so send message will expire.
  task_environment_.FastForwardBy(kTimeout);
  EXPECT_EQ(SharingSendMessageResult::kAckTimeout, send_message_result());

  // Simulate ack message received by AckMessageHandler, which will be
  // disregarded.
  SharingMessageHandler* ack_message_handler = fcm_handler_->GetSharingHandler(
      chrome_browser_sharing::SharingMessage::kAckMessage);
  EXPECT_TRUE(ack_message_handler);

  chrome_browser_sharing::SharingMessage ack_message;
  ack_message.mutable_ack_message()->set_original_message_id(kMessageId);
  ack_message_handler->OnMessage(ack_message, base::DoNothing());

  EXPECT_EQ(SharingSendMessageResult::kAckTimeout, send_message_result());
}

TEST_F(SharingServiceTest, SendMessageToDeviceExpired) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(id, kDeviceName);
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(device_info.get());
  fake_device_info_sync_service.GetLocalDeviceInfoProvider()
      ->GetMutableDeviceInfo()
      ->set_sharing_info(CreateLocalSharingInfo());
  sync_prefs_->SetFCMRegistration(SharingSyncPreference::FCMRegistration(
      kAuthorizedEntity, base::Time::Now()));

  GetSharingService()->SendMessageToDevice(
      id, kTimeout, chrome_browser_sharing::SharingMessage(),
      base::BindOnce(&SharingServiceTest::OnMessageSent,
                     base::Unretained(this)));

  EXPECT_EQ(kP256dh, fake_gcm_driver_.p256dh());
  EXPECT_EQ(kAuthSecret, fake_gcm_driver_.auth_secret());
  EXPECT_EQ(kFcmToken, fake_gcm_driver_.fcm_token());

  // Advance time so send message will expire.
  task_environment_.FastForwardBy(kSendMessageTimeout);
  EXPECT_EQ(SharingSendMessageResult::kAckTimeout, send_message_result());

  // Simulate ack message received by AckMessageHandler, which will be
  // disregarded.
  SharingMessageHandler* ack_message_handler = fcm_handler_->GetSharingHandler(
      chrome_browser_sharing::SharingMessage::kAckMessage);
  EXPECT_TRUE(ack_message_handler);

  chrome_browser_sharing::SharingMessage ack_message;
  ack_message.mutable_ack_message()->set_original_message_id(kMessageId);
  ack_message_handler->OnMessage(ack_message, base::DoNothing());

  EXPECT_EQ(SharingSendMessageResult::kAckTimeout, send_message_result());
}

TEST_F(SharingServiceTest, DeviceRegistration) {
  // Enable the feature.
  scoped_feature_list_.InitAndEnableFeature(kSharingDeviceRegistration);
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes(
      {syncer::DEVICE_INFO, syncer::PREFERENCES});

  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Expect registration to be successful on sync state changed.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kSuccess);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  // As device is already registered, won't attempt registration anymore.
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(0);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  auto vapid_key = crypto::ECPrivateKey::Create();
  ASSERT_TRUE(vapid_key);
  std::vector<uint8_t> vapid_key_info;
  ASSERT_TRUE(vapid_key->ExportPrivateKey(&vapid_key_info));

  // Registration will be attempeted as VAPID key has changed.
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(0);
  sync_prefs_->SetVapidKey(vapid_key_info);
  EXPECT_EQ(2, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, DeviceRegistrationPreferenceNotAvailable) {
  // Enable the feature.
  scoped_feature_list_.InitAndEnableFeature(kSharingDeviceRegistration);
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes(syncer::DEVICE_INFO);

  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // As sync preferences is not available, registration shouldn't start.
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(0);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(0, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, DeviceRegistrationTransportMode) {
  // Enable the registration feature and transport mode required features.
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{kSharingDeviceRegistration, kSharingUseDeviceInfo,
                            kSharingDeriveVapidKey},
      /*disabled_features=*/{});
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes(syncer::DEVICE_INFO);
  test_sync_service_.SetExperimentalAuthenticationKey(
      crypto::ECPrivateKey::Create());

  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Expect registration to be successful on sync state changed.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kSuccess);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  // Registration will be attempeted as sync auth id has changed.
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(0);
  test_sync_service_.SetExperimentalAuthenticationKey(
      crypto::ECPrivateKey::Create());
  test_sync_service_.FireSyncCycleCompleted();
  EXPECT_EQ(2, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, DeviceRegistrationTransientError) {
  // Enable the feature.
  scoped_feature_list_.InitAndEnableFeature(kSharingDeviceRegistration);
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes(
      {syncer::DEVICE_INFO, syncer::PREFERENCES});

  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Retry will be scheduled on transient error received.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kFcmTransientError);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::REGISTERING,
            GetSharingService()->GetStateForTesting());

  // Retry should be scheduled by now. Next retry after 5 minutes will be
  // successful.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kSuccess);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  task_environment_.FastForwardBy(
      base::TimeDelta::FromMilliseconds(kRetryBackoffPolicy.initial_delay_ms));
  EXPECT_EQ(2, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, DeviceUnregistrationFeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kSharingDeviceRegistration);
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kSuccess);

  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Further state changes are ignored.
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, DeviceUnregistrationSyncDisabled) {
  scoped_feature_list_.InitAndEnableFeature(kSharingDeviceRegistration);
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::DISABLED);

  // Create new SharingService instance with sync disabled at constructor.
  GetSharingService();
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, DeviceRegisterAndUnregister) {
  // Enable the feature.
  scoped_feature_list_.InitAndEnableFeature(kSharingDeviceRegistration);
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes(
      {syncer::DEVICE_INFO, syncer::PREFERENCES});

  // Create new SharingService instance with feature enabled at constructor.
  GetSharingService();
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Expect registration to be successful on sync state changed.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kSuccess);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(0, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  // Further state changes do nothing.
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(0);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(0, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  // Change sync to configuring, which will be ignored.
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::CONFIGURING);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(0, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  // Disable sync and un-registration should happen.
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::DISABLED);
  EXPECT_CALL(*fcm_handler_, StopListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Further state changes do nothing.
  EXPECT_CALL(*fcm_handler_, StopListening()).Times(0);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Should be able to register once again when sync is back on.
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(2, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  // Disable syncing of preference and un-registration should happen.
  test_sync_service_.SetActiveDataTypes(syncer::DEVICE_INFO);
  EXPECT_CALL(*fcm_handler_, StopListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(2, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(2, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, StartListeningToFCMAtConstructor) {
  scoped_feature_list_.InitAndEnableFeature(kSharingDeviceRegistration);
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes(
      {syncer::DEVICE_INFO, syncer::PREFERENCES});

  // Create new SharingService instance with FCM already registered at
  // constructor.
  sync_prefs_->SetFCMRegistration(SharingSyncPreference::FCMRegistration(
      kAuthorizedEntity, base::Time::Now()));
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
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(device_info.get());

  std::vector<std::unique_ptr<syncer::DeviceInfo>> candidates =
      GetSharingService()->GetDeviceCandidates(
          sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  ASSERT_EQ(0u, candidates.size());
}

TEST_F(SharingServiceTest, DeviceCandidatesAlreadyReady) {
  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(id, kDeviceName);
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(device_info.get());
  fake_device_info_sync_service.GetLocalDeviceInfoProvider()->SetReady(true);

  GetSharingService()->AddDeviceCandidatesInitializedObserver(
      base::BindOnce(&SharingServiceTest::OnDeviceCandidatesInitialized,
                     base::Unretained(this)));

  ASSERT_TRUE(device_candidates_initialized_);
}

TEST_F(SharingServiceTest, DeviceCandidatesReadyAfterAddObserver) {
  fake_device_info_sync_service.GetLocalDeviceInfoProvider()->SetReady(false);

  GetSharingService()->AddDeviceCandidatesInitializedObserver(
      base::BindOnce(&SharingServiceTest::OnDeviceCandidatesInitialized,
                     base::Unretained(this)));

  ASSERT_FALSE(device_candidates_initialized_);

  std::string id = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(id, kDeviceName);
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(device_info.get());
  fake_device_info_sync_service.GetLocalDeviceInfoProvider()->SetReady(true);

  ASSERT_TRUE(device_candidates_initialized_);
}

TEST_F(SharingServiceTest, DeviceCandidatesNames_Computers) {
  std::unique_ptr<syncer::DeviceInfo> computer1 = CreateFakeDeviceInfo(
      base::GenerateGUID(), "Fake device 1",
      sync_pb::SyncEnums_DeviceType_TYPE_WIN, {"Dell", "PC3999", "sno one"});

  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(computer1.get());
  std::vector<std::unique_ptr<syncer::DeviceInfo>> candidates =
      GetSharingService()->GetDeviceCandidates(
          sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  ASSERT_EQ(1u, candidates.size());
  EXPECT_EQ("Dell Computer", candidates[0]->client_name());

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));

  std::unique_ptr<syncer::DeviceInfo> computer2 = CreateFakeDeviceInfo(
      base::GenerateGUID(), "Fake device 2",
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX, {"Dell", "PC3998", "sno two"});

  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(computer2.get());
  candidates = GetSharingService()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  ASSERT_EQ(2u, candidates.size());
  EXPECT_EQ("Dell Computer PC3998", candidates[0]->client_name());
  EXPECT_EQ("Dell Computer PC3999", candidates[1]->client_name());
}

TEST_F(SharingServiceTest, DeviceCandidatesNames_AppleDevices) {
  std::unique_ptr<syncer::DeviceInfo> computer1 =
      CreateFakeDeviceInfo(base::GenerateGUID(), "Fake device 1",
                           sync_pb::SyncEnums_DeviceType_TYPE_TABLET,
                           {"Apple Inc.", "iPad2,2", "sno one"});
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(computer1.get());

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));

  std::unique_ptr<syncer::DeviceInfo> computer2 =
      CreateFakeDeviceInfo(base::GenerateGUID(), "Fake device 2",
                           sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
                           {"Apple Inc.", "iPhone1,1", "sno two"});
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(computer2.get());

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));

  std::unique_ptr<syncer::DeviceInfo> computer3 =
      CreateFakeDeviceInfo(base::GenerateGUID(), "Fake device 3",
                           sync_pb::SyncEnums_DeviceType_TYPE_MAC,
                           {"Apple Inc.", "MacbookPro1,1", "sno three"});
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(computer3.get());

  std::vector<std::unique_ptr<syncer::DeviceInfo>> candidates =
      GetSharingService()->GetDeviceCandidates(
          sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  ASSERT_EQ(3u, candidates.size());
  EXPECT_EQ("MacbookPro", candidates[0]->client_name());
  EXPECT_EQ("iPhone", candidates[1]->client_name());
  EXPECT_EQ("iPad", candidates[2]->client_name());
}

TEST_F(SharingServiceTest, DeviceCandidatesNames_AndroidDevices) {
  std::unique_ptr<syncer::DeviceInfo> computer1 =
      CreateFakeDeviceInfo(base::GenerateGUID(), "Fake device 1",
                           sync_pb::SyncEnums_DeviceType_TYPE_TABLET,
                           {"Google", "Pixel Slate", "sno one"});
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(computer1.get());

  std::vector<std::unique_ptr<syncer::DeviceInfo>> candidates =
      GetSharingService()->GetDeviceCandidates(
          sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  ASSERT_EQ(1u, candidates.size());
  EXPECT_EQ("Google Tablet", candidates[0]->client_name());

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));

  std::unique_ptr<syncer::DeviceInfo> computer2 =
      CreateFakeDeviceInfo(base::GenerateGUID(), "Fake device 2",
                           sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
                           {"Google", "Pixel 3", "sno two"});
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(computer2.get());

  candidates = GetSharingService()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  ASSERT_EQ(2u, candidates.size());
  EXPECT_EQ("Google Phone", candidates[0]->client_name());
  EXPECT_EQ("Google Tablet", candidates[1]->client_name());

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));

  std::unique_ptr<syncer::DeviceInfo> computer3 =
      CreateFakeDeviceInfo(base::GenerateGUID(), "Fake device 3",
                           sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
                           {"Google", "Pixel 2", "sno three"});
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(computer3.get());

  candidates = GetSharingService()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  ASSERT_EQ(3u, candidates.size());
  EXPECT_EQ("Google Phone Pixel 2", candidates[0]->client_name());
  EXPECT_EQ("Google Phone Pixel 3", candidates[1]->client_name());
  EXPECT_EQ("Google Tablet", candidates[2]->client_name());
}

TEST_F(SharingServiceTest, DeviceCandidatesNames_Chromebooks) {
  std::unique_ptr<syncer::DeviceInfo> computer1 =
      CreateFakeDeviceInfo(base::GenerateGUID(), "Fake device 1",
                           sync_pb::SyncEnums_DeviceType_TYPE_CROS,
                           {"Dell", "Chromebook", "sno one"});
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(computer1.get());

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));

  std::unique_ptr<syncer::DeviceInfo> computer2 = CreateFakeDeviceInfo(
      base::GenerateGUID(), "Fake device 2",
      sync_pb::SyncEnums_DeviceType_TYPE_CROS, {"HP", "Chromebook", "sno one"});
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(computer2.get());

  std::vector<std::unique_ptr<syncer::DeviceInfo>> candidates =
      GetSharingService()->GetDeviceCandidates(
          sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  ASSERT_EQ(2u, candidates.size());
  EXPECT_EQ("HP Chromebook", candidates[0]->client_name());
  EXPECT_EQ("Dell Chromebook", candidates[1]->client_name());
}

TEST_F(SharingServiceTest, GetDeviceByGuid) {
  std::string guid = base::GenerateGUID();
  std::unique_ptr<syncer::DeviceInfo> computer1 = CreateFakeDeviceInfo(
      guid, "Fake device 1", sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
      {"Dell", "sno one", "serial no"});
  fake_device_info_sync_service.GetDeviceInfoTracker()->Add(computer1.get());

  std::unique_ptr<syncer::DeviceInfo> device_info =
      GetSharingService()->GetDeviceByGuid(guid);
  EXPECT_EQ("Dell Computer sno one", device_info->client_name());
}
