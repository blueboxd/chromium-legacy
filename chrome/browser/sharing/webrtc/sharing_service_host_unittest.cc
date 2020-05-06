// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/webrtc/sharing_service_host.h"

#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/sharing/mock_sharing_device_source.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/webrtc/sharing_webrtc_connection_host.h"
#include "chrome/services/sharing/public/mojom/webrtc.mojom.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeSharingMojoService : public sharing::mojom::Sharing,
                               public sharing::mojom::SignallingReceiver {
 public:
  using NearbyConnectionsHostMojom =
      location::nearby::connections::mojom::NearbyConnectionsHost;

  FakeSharingMojoService() = default;
  ~FakeSharingMojoService() override = default;

  // sharing::mojom::Sharing:
  void CreateSharingWebRtcConnection(
      mojo::PendingRemote<sharing::mojom::SignallingSender> signalling_sender,
      mojo::PendingReceiver<sharing::mojom::SignallingReceiver>
          signalling_receiver,
      mojo::PendingRemote<sharing::mojom::SharingWebRtcConnectionDelegate>
          delegate,
      mojo::PendingReceiver<sharing::mojom::SharingWebRtcConnection> connection,
      mojo::PendingRemote<network::mojom::P2PSocketManager> socket_manager,
      mojo::PendingRemote<network::mojom::MdnsResponder> mdns_responder,
      std::vector<sharing::mojom::IceServerPtr> ice_servers) override {
    signaling_set.Add(this, std::move(signalling_receiver));
  }
  void CreateNearbyConnections(
      mojo::PendingRemote<NearbyConnectionsHostMojom> host,
      CreateNearbyConnectionsCallback callback) override {
    NOTIMPLEMENTED();
  }

  void OnOfferReceived(const std::string& offer,
                       OnOfferReceivedCallback callback) override {
    std::move(callback).Run(answer);
  }

  void OnIceCandidatesReceived(
      std::vector<sharing::mojom::IceCandidatePtr> ice_candidates) override {}

  mojo::Receiver<sharing::mojom::Sharing> receiver{this};
  mojo::ReceiverSet<sharing::mojom::SignallingReceiver> signaling_set;
  std::string answer;
};

}  // namespace

class SharingServiceHostTest : public testing::Test {
 public:
  SharingServiceHostTest() {
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());

    host_ = std::make_unique<SharingServiceHost>(
        /*message_sender=*/nullptr,
        /*gcm_driver=*/nullptr, &sync_prefs_, &device_source_,
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));

    host_->BindSharingServiceForTesting(
        fake_mojo_service_.receiver.BindNewPipeAndPassRemote());
  }

  std::string GetApiUrl() const {
    return base::StrCat(
        {"https://networktraversal.googleapis.com/v1alpha/iceconfig?key=",
         google_apis::GetSharingAPIKey()});
  }

  std::string OnOfferReceivedSync(const std::string& guid) {
    std::string result;
    base::RunLoop run_loop;
    host_->OnOfferReceived(
        guid, /*fcm_configuration=*/{}, "offer",
        base::BindLambdaForTesting([&](const std::string& answer) {
          result = answer;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      content::BrowserTaskEnvironment::MainThreadType::IO};
  MockSharingDeviceSource device_source_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service_;
  SharingSyncPreference sync_prefs_{&prefs_, &fake_device_info_sync_service_};
  network::TestURLLoaderFactory test_url_loader_factory_;
  FakeSharingMojoService fake_mojo_service_;

  std::unique_ptr<SharingServiceHost> host_;
};

TEST_F(SharingServiceHostTest, OnIceCandidatesReceived_BeforeConnection) {
  host_->OnIceCandidatesReceived("unknown_guid", {}, {});
  // Receiving an IceCandidate should not open new connections.
  EXPECT_TRUE(host_->GetConnectionsForTesting().empty());
}

TEST_F(SharingServiceHostTest, OnOfferReceived_BeforeConnection) {
  std::string guid = "unknown_guid";
  std::string answer = "answer";

  fake_mojo_service_.answer = answer;

  EXPECT_CALL(device_source_, GetDeviceByGuid(guid))
      .WillOnce(testing::Return(testing::ByMove(nullptr)));
  test_url_loader_factory_.AddResponse(GetApiUrl(), /*response=*/std::string(),
                                       net::HTTP_OK);

  EXPECT_EQ(answer, OnOfferReceivedSync(guid));
  // Receiving an offer should initialize a WebRTC connection.
  EXPECT_EQ(1u, host_->GetConnectionsForTesting().size());
}

TEST_F(SharingServiceHostTest, OnOfferReceived_ExistingConnection) {
  std::string guid = "existing_guid";
  fake_mojo_service_.answer = "answer";

  // Simulate existing connection for |guid|
  host_->GetConnectionsForTesting()[guid] = nullptr;

  EXPECT_EQ("", OnOfferReceivedSync(guid));

  // We should still have only one connection
  auto& connections = host_->GetConnectionsForTesting();
  EXPECT_EQ(1u, connections.size());
  EXPECT_TRUE(connections.find(guid) != connections.end());
}
