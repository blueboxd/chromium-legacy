// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_per_session_discovery_manager.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/nearby_sharing/mock_nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "chrome/browser/nearby_sharing/transfer_metadata_builder.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;

MATCHER_P(MatchesTarget, target, "") {
  return arg.id == target.id;
}

class MockShareTargetListener
    : public nearby_share::mojom::ShareTargetListener {
 public:
  MockShareTargetListener() = default;
  ~MockShareTargetListener() override = default;

  mojo::PendingRemote<ShareTargetListener> Bind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // nearby_share::mojom::ShareTargetListener:
  MOCK_METHOD(void, OnShareTargetDiscovered, (const ShareTarget&), (override));
  MOCK_METHOD(void, OnShareTargetLost, (const ShareTarget&), (override));

 private:
  mojo::Receiver<ShareTargetListener> receiver_{this};
};

class NearbyPerSessionDiscoveryManagerTest : public testing::Test {
 public:
  using MockSelectShareTargetCallback = base::MockCallback<
      NearbyPerSessionDiscoveryManager::SelectShareTargetCallback>;
  using MockStartDiscoveryCallback = base::MockCallback<
      NearbyPerSessionDiscoveryManager::StartDiscoveryCallback>;

  NearbyPerSessionDiscoveryManagerTest() = default;
  ~NearbyPerSessionDiscoveryManagerTest() override = default;

  MockNearbySharingService& sharing_service() { return sharing_service_; }

  NearbyPerSessionDiscoveryManager& manager() { return manager_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  MockNearbySharingService sharing_service_;
  NearbyPerSessionDiscoveryManager manager_{&sharing_service_};
};

}  // namespace

TEST_F(NearbyPerSessionDiscoveryManagerTest, CreateDestroyWithoutRegistering) {
  EXPECT_CALL(sharing_service(), RegisterSendSurface(&manager(), &manager(), _))
      .Times(0);
  EXPECT_CALL(sharing_service(), UnregisterSendSurface(&manager(), &manager()))
      .Times(0);
  {
    NearbyPerSessionDiscoveryManager manager(&sharing_service());
    // Creating and destroying an instance should not register itself with the
    // NearbySharingService.
  }
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, StartDiscovery_Success) {
  MockStartDiscoveryCallback callback;
  EXPECT_CALL(callback, Run(/*success=*/true));

  EXPECT_CALL(
      sharing_service(),
      RegisterSendSurface(&manager(), &manager(),
                          NearbySharingService::SendSurfaceState::kForeground))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));
  EXPECT_CALL(sharing_service(), UnregisterSendSurface(&manager(), &manager()))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));

  MockShareTargetListener listener;
  manager().StartDiscovery(listener.Bind(), callback.Get());
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, StartDiscovery_Error) {
  MockStartDiscoveryCallback callback;
  EXPECT_CALL(callback, Run(/*success=*/false));

  EXPECT_CALL(
      sharing_service(),
      RegisterSendSurface(&manager(), &manager(),
                          NearbySharingService::SendSurfaceState::kForeground))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kError));
  EXPECT_CALL(sharing_service(), UnregisterSendSurface(&manager(), &manager()))
      .Times(0);

  MockShareTargetListener listener;
  manager().StartDiscovery(listener.Bind(), callback.Get());
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, OnShareTargetDiscovered) {
  MockShareTargetListener listener;
  EXPECT_CALL(sharing_service(),
              RegisterSendSurface(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));

  manager().StartDiscovery(listener.Bind(), base::DoNothing());

  ShareTarget share_target;

  base::RunLoop run_loop;
  EXPECT_CALL(listener, OnShareTargetDiscovered(MatchesTarget(share_target)))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  manager().OnShareTargetDiscovered(share_target);

  run_loop.Run();
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, OnShareTargetLost) {
  MockShareTargetListener listener;
  EXPECT_CALL(sharing_service(),
              RegisterSendSurface(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));

  manager().StartDiscovery(listener.Bind(), base::DoNothing());

  ShareTarget share_target;

  base::RunLoop run_loop;
  EXPECT_CALL(listener, OnShareTargetLost(MatchesTarget(share_target)))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  manager().OnShareTargetLost(share_target);

  run_loop.Run();
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, SelectShareTarget_Invalid) {
  MockShareTargetListener listener;
  EXPECT_CALL(sharing_service(),
              RegisterSendSurface(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));
  manager().StartDiscovery(listener.Bind(), base::DoNothing());

  MockSelectShareTargetCallback callback;
  EXPECT_CALL(
      callback,
      Run(nearby_share::mojom::SelectShareTargetResult::kInvalidShareTarget,
          testing::Eq(base::nullopt), testing::IsFalse()));

  manager().SelectShareTarget({}, callback.Get());
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, SelectShareTarget_SendSuccess) {
  // Setup share target
  MockShareTargetListener listener;
  EXPECT_CALL(sharing_service(),
              RegisterSendSurface(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));

  manager().StartDiscovery(listener.Bind(), base::DoNothing());
  ShareTarget share_target;
  manager().OnShareTargetDiscovered(share_target);

  // We don't expect the callback to be called until OnTransferUpdate().
  MockSelectShareTargetCallback callback;
  EXPECT_CALL(callback, Run(_, _, _)).Times(0);

  // TODO(crbug.com/1099710): Call correct method and pass attachments.
  EXPECT_CALL(sharing_service(), SendText(_, _, _))
      .WillOnce(testing::Invoke(
          [&share_target](const ShareTarget& target, std::string text,
                          NearbySharingService::StatusCodesCallback callback) {
            EXPECT_EQ(share_target.id, target.id);
            std::move(callback).Run(NearbySharingService::StatusCodes::kOk);
          }));

  manager().SelectShareTarget(share_target.id, callback.Get());
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, SelectShareTarget_SendError) {
  // Setup share target
  MockShareTargetListener listener;
  EXPECT_CALL(sharing_service(),
              RegisterSendSurface(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));

  manager().StartDiscovery(listener.Bind(), base::DoNothing());
  ShareTarget share_target;
  manager().OnShareTargetDiscovered(share_target);

  // Expect an error if NearbySharingService::Send*() fails.
  MockSelectShareTargetCallback callback;
  EXPECT_CALL(callback,
              Run(nearby_share::mojom::SelectShareTargetResult::kError,
                  testing::Eq(base::nullopt), testing::IsFalse()));

  // TODO(crbug.com/1099710): Call correct method and pass attachments.
  EXPECT_CALL(sharing_service(), SendText(_, _, _))
      .WillOnce(testing::Invoke(
          [&share_target](const ShareTarget& target, std::string text,
                          NearbySharingService::StatusCodesCallback callback) {
            EXPECT_EQ(share_target.id, target.id);
            std::move(callback).Run(NearbySharingService::StatusCodes::kError);
          }));

  manager().SelectShareTarget(share_target.id, callback.Get());
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, OnTransferUpdate_WaitRemote) {
  // Setup share target
  MockShareTargetListener listener;
  EXPECT_CALL(sharing_service(),
              RegisterSendSurface(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));

  manager().StartDiscovery(listener.Bind(), base::DoNothing());
  ShareTarget share_target;
  manager().OnShareTargetDiscovered(share_target);

  // Expect success without confirmation manager and token.
  MockSelectShareTargetCallback callback;
  EXPECT_CALL(callback, Run(nearby_share::mojom::SelectShareTargetResult::kOk,
                            testing::Eq(base::nullopt), testing::IsFalse()));

  manager().SelectShareTarget(share_target.id, callback.Get());

  auto metadata =
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kAwaitingRemoteAcceptance)
          .build();
  manager().OnTransferUpdate(share_target, metadata);
}

TEST_F(NearbyPerSessionDiscoveryManagerTest, OnTransferUpdate_WaitLocal) {
  // Setup share target
  MockShareTargetListener listener;
  EXPECT_CALL(sharing_service(),
              RegisterSendSurface(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(NearbySharingService::StatusCodes::kOk));

  manager().StartDiscovery(listener.Bind(), base::DoNothing());
  ShareTarget share_target;
  manager().OnShareTargetDiscovered(share_target);

  std::string token = "Test Token";

  // Expect to get a valid confirmation manager remote and token.
  MockSelectShareTargetCallback callback;
  EXPECT_CALL(callback, Run(nearby_share::mojom::SelectShareTargetResult::kOk,
                            testing::Eq(token), testing::IsTrue()));

  manager().SelectShareTarget(share_target.id, callback.Get());

  auto metadata =
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kAwaitingLocalConfirmation)
          .set_token(token)
          .build();
  manager().OnTransferUpdate(share_target, metadata);
}
